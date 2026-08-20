// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <blk.h>
#include <config.h>
#include <exports.h>
#include <fs.h>
#include <asm/byteorder.h>
#include <part.h>
#include <malloc.h>
#include <memalign.h>
#include <linux/compiler.h>
#include <linux/ctype.h>
#include <isofs.h>
#include <part_iso.h>

#define	ISO_ROOT_DIRECTORY_RECORD_SIZE	34

static unsigned char tmpbuf[CD_SECTSIZE] __aligned(ARCH_DMA_MINALIGN);
static struct blk_desc *cur_dev = NULL;
static unsigned char pvd[CD_SECTSIZE];
static int joliet = 0;

static int ucs2_to_ascii(const char *in, int insize, char *out, int outsize)
{
	int i, n = 0;
	uint16_t ucs2_code;

	for (i = 0; i < insize; i += 2) {
		if (n >= outsize)
			break;
		
		ucs2_code = (in[i] << 8) | in[i+1];
		if (ucs2_code < 0x80) {
			out[n++] = (char)ucs2_code;
		} else {
			out[n++] = '?';
		}
	}

	return n;
}

static int iso_match(const char *path, iso_directory_record_t *dir_rec)
{
	char *name;
	char ascii_name[256];
	int i, n;

	if (joliet) {
		n = ucs2_to_ascii(dir_rec->name, dir_rec->namelen, ascii_name, 256);
		name = (char*)ascii_name;
	} else {
		name = dir_rec->name;
		n = dir_rec->namelen;
	}
	
	for (i = n; --i >= 0 && *path != 0 && *path != '/'; path++, name++) {
		if (toupper(*path) != toupper(*name))
			return 0;
	}

	if (*path && *path != '/')
		return 0;
	
	/*
	 * Allow stripping of trailing dots and the version number.
	 * Note that this will find the first instead of the last version
	 * of a file.
	 */
	if (i >= 0 && (*name == ';' || *name == '.')) {
		/* This is to prevent matching of numeric extensions */
		if (*name == '.' && name[1] != ';')
			return 0;
		while (--i >= 0)
			if (*++name != ';' && (*name < '0' || *name > '9'))
				return 0;
	}
	return 1;
}

static iso_directory_record_t* iso_get_rootdir(void *vd)
{
	if (!vd || *((unsigned char*)vd) == ISO_VD_END)
		return NULL;
	
	return (iso_directory_record_t*)(vd + 156);
}

static void __maybe_unused print_directory_record_info(iso_directory_record_t *dr)
{
	char ascii_name[256];
	char *name;
	int i, n;

	printf("+++ iso directory record: [\n");
	printf("  length: %d\n", dr->length);
	printf("  extloc_LE: %d\n", le32_to_cpu(dr->extloc_LE));
	printf("  extlen_LE: %d\n", le32_to_cpu(dr->extlen_LE));
	printf("  flags: 0x%x\n", dr->flags);
	printf("  file_unit_size: %d\n", dr->file_unit_size);
	printf("  interleave: %d\n", dr->interleave);
	printf("  volume_sequence_number_LE: %d\n", le16_to_cpu(dr->volume_sequence_number_LE));
	printf("  namelen: %u\n", dr->namelen);
	printf("  name: ");
	if (joliet) {
		n = ucs2_to_ascii(dr->name, dr->namelen, ascii_name, 256);
		name = (char*)ascii_name;
	} else {
		name = (char*)dr->name;
		n = dr->namelen;
	}
	for (i = 0; i < n; i++) {
		printf("%c", isprint(name[i]) ? name[i] : '?');
	}
	printf("\n]\n");
}

int isofs_probe(struct blk_desc *dev_desc, struct disk_partition *info)
{
	char retry_times = 0;
	lbaint_t blkno;
	int level1_achieve = 0;
	unsigned short sectsz = CD_SECTSIZE;

	if (CD_SECTSIZE % dev_desc->blksz != 0) {
		printf("Not support the block size: %ld\n", dev_desc->blksz);
		return -EINVAL;
	}

	memset(pvd, 0, CD_SECTSIZE);
	pvd[0] = ISO_VD_END;

	/* Find the volume descriptor */
	cur_dev = dev_desc;

	for (blkno = PVD_OFFSET; blkno < PVD_OFFSET + 100; blkno++) {
again:
		if (iso_dread(dev_desc, blkno, 1, tmpbuf) != 1) {
			printf("iso9660: read Volume Descriptor failed\n");
			goto out;
		}

		if (strncmp((char *)tmpbuf + 1, ISO_STANDARD_ID, 5) != 0) {
			if (retry_times++ < 5) {
					udelay(10000);
				goto again;
			} else {
					printf("iso9660: Volume Descriptor is err\n");
				goto out;
			}
		}
		if (tmpbuf[0] == ISO_VD_END) {
			// if achieve level1 also mean this isofs available
			if (level1_achieve)
				break;
			printf("iso9660: can not find Primary Volume Descriptor\n");
			goto out;
		}

		if (tmpbuf[0] == ISO_VD_PRIMARY) {
			memcpy(pvd, tmpbuf, CD_SECTSIZE);
			level1_achieve = 1; // mean this isofs achieve level1
			continue;
		} else if (tmpbuf[0] == ISO_VD_SUPPLEMENTARY) {
			debug("ISO 9660 Extensions: Microsoft Joliet\n");
			memcpy(pvd, tmpbuf, CD_SECTSIZE);
			joliet = 1;
			break;
		}
	}

	// pvd is last volume before Terminator(0xff)
	sectsz = *((unsigned short *)(&pvd[128]));
	if (le16_to_cpu(sectsz) != CD_SECTSIZE) {
		printf("iso9660: Unsupported block size %u\n", le16_to_cpu(sectsz));
		goto out;
	}
	
	return 0;

out:
	cur_dev = NULL;
	joliet = 0;
	return -1;
}

void isofs_close(void)
{
	cur_dev = NULL;
	joliet = 0;
}

static int isofs_lookup_directory_record(const char *path, iso_directory_record_t *dir_rec)
{
	const char *cur, *next, *end;
	iso_directory_record_t *dr;
	lbaint_t blkno, blkcnt;
	unsigned int size;
	int rc;

	if (!path || path[0] == '\0')
		return -EINVAL;

	dr = iso_get_rootdir(pvd);
	if (!dr)
		return -ENOENT;

	if (path[0] == '/' && path[1] == '\0') {
		if (dir_rec)
			memcpy(dir_rec, dr, ISO_ROOT_DIRECTORY_RECORD_SIZE);
		return 0;
	}

	end = path + strlen(path);
	cur = path;
	if (cur[0] == '/')
		++cur;

	while (cur < end) {
		blkno = le32_to_cpu(dr->extloc_LE);
		size = le32_to_cpu(dr->extlen_LE);
		blkcnt = DIV_ROUND_UP(size, CD_SECTSIZE);
		rc = iso_dread(cur_dev, blkno, blkcnt, tmpbuf);
		if (rc != blkcnt)
			return -EIO;

		dr = (iso_directory_record_t*)tmpbuf;
		while ((void*)dr < (void*)tmpbuf + size) {
			if (dr->length == 0)
				break;

			if (iso_match(cur, dr)) {
				next = strchr(cur, '/');
				if (!next)
					cur = end;
				else
					cur = next + 1;
				break;
			}

			dr = (iso_directory_record_t*)((void*)dr + dr->length);
		}

		if ((void*)dr >= (void*)tmpbuf + size)
			return -ENOENT;
	}

	if (dir_rec)
		memcpy(dir_rec, dr, dr->length);
	
	return 0;
}

int isofs_read_file(const char *path, void *buffer, loff_t offset, loff_t len,
		  loff_t *actread)
{
	iso_directory_record_t dr;
	loff_t size, asked, n = 0;
	lbaint_t bno, bcnt;
	int rc;

	if (pvd[0] == ISO_VD_END) {
		printf("No Primary Volume Descriptor\n");
		return -EINVAL;
	}

	rc = isofs_lookup_directory_record(path, &dr);
	if (rc) {
		printf("lookup dir %s failed\n", path);
		return rc;
	}

	// print_directory_record_info(&dr);

	size = le32_to_cpu(dr.extlen_LE);
	if (offset < 0 || offset >= size)
		return -EINVAL;

	if (len == 0 || len > size)
		len = size;

	asked = len;
	bno = le32_to_cpu(dr.extloc_LE);

	// handle the offset not align CD_SECTSIZE.
	if (offset > 0) {
		bno += offset / CD_SECTSIZE;
		offset = offset % CD_SECTSIZE;
		rc = iso_dread(cur_dev, bno, 1, tmpbuf);
		if (rc != 1) {
			return -EIO;
		}

		if (offset + len > CD_SECTSIZE)
			n = CD_SECTSIZE - offset;
		else
			n = len;
		memcpy(buffer, tmpbuf + offset, n);
		buffer += n;
		len -= n;
		bno++;
	}

	if (len <= 0)
		goto finish;

	// bluk read.
	bcnt = len / CD_SECTSIZE;
	if (bcnt > 0) {
		rc = iso_dread(cur_dev, bno, bcnt, buffer);
		if (rc != bcnt) {
			return -EIO;
		}
		n = bcnt * CD_SECTSIZE;
		buffer += n;
		len -= n;
		bno += bcnt;
	}
        
        // handle the remaining data. 
	if (len > 0) {
		rc = iso_dread(cur_dev, bno, 1, tmpbuf);
		if (rc != 1) {
			return -EIO;
		}
		n = len;
		memcpy(buffer, tmpbuf, n);
		buffer += n;
		len -= n;
		bno++;
	}

finish:
	if (actread)
		*actread = asked - len;

	return 0;
}

int isofs_size(const char *path, loff_t *size)
{
	iso_directory_record_t dr;
	int rc;

	rc = isofs_lookup_directory_record(path, &dr);
	if (rc) {
		printf("get Directory Record failed\n");
		*size = 0;
		return -EIO;
	}

	*size = le32_to_cpu(dr.extlen_LE);
	return 0;
}

int isofs_ls(const char *path)
{
	iso_directory_record_t dr;
	iso_directory_record_t *drp;
	lbaint_t blkno, blkcnt;
	unsigned int size;
	int rc, i, n, fcnt = 0, dcnt = 0;
	char ascii_name[256];
	char *name;
	int is_root = 0;

	if (path[0] == '/' && path[1] == '\0')
		is_root = 1;

	rc = isofs_lookup_directory_record(path, &dr);
	if (rc) {
		printf("lookup dir %s failed\n", path);
		return rc;
	}

	// print_directory_record_info(&dr);

	blkno = le32_to_cpu(dr.extloc_LE);
	size = le32_to_cpu(dr.extlen_LE);
	blkcnt = DIV_ROUND_UP(size, CD_SECTSIZE);
	rc = iso_dread(cur_dev, blkno, blkcnt, tmpbuf);
	if (rc != blkcnt)
		return -EIO;

	drp = (iso_directory_record_t*)tmpbuf;
	while ((void*)drp < (void*)tmpbuf + size) {
		if (drp->length == 0)
			break;

		if (drp->namelen == 1 && (drp->name[0] == 0 || drp->name[0] == 1)) {
			if (!is_root && drp->name[0] == 0) 
				printf("                ./\n");
			else if (!is_root && drp->name[0] == 1)
				printf("                ../\n");

			drp = (iso_directory_record_t*)((void*)drp + drp->length);
			continue;
		}

		if (joliet) {
			n = ucs2_to_ascii(drp->name, drp->namelen, ascii_name, 256);
			name = (char*)ascii_name;
		} else {
			name = (char*)drp->name;
			n = drp->namelen;
		}

		if (drp->flags & 0x2) { // is dir
			dcnt++;
			printf("                ");
		} else {
			fcnt++;
			printf(" %12u   ", le32_to_cpu(drp->extlen_LE));
		}

		for (i = 0; i < n; i++) {
			if (name[i] == ';' || (name[i] == '.' && name[i + 1] == ';'))
				break;
			printf("%c", isprint(name[i]) ? name[i] : '?');
		}

		if (drp->flags & 0x2) // is dir
			printf("/");

		printf("\n");
		drp = (iso_directory_record_t*)((void*)drp + drp->length);
	}

	printf("\n    %d file(s), %d dir(s)\n\n", fcnt, dcnt);
	return 0;
}

