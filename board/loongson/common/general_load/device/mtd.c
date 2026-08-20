#include <blk.h>
#include <gzip.h>
#include <linux/kernel.h>
#include <memalign.h>
#include <watchdog.h>
#include <u-boot/zlib.h>
#include "mtd.h"
#include "../gl_debug.h"

static int m__mtd_gl_erase_mapped(struct mtd_gl_desc* self, u64 addr)
{
	struct erase_info einfo;
	memset(&einfo, 0, sizeof(einfo));

	einfo.mtd = self->mtd;
	einfo.len = self->mtd->erasesize;
	einfo.addr = addr + self->badsize;

	GL_PRINTF("mtd_gl_erase 0x%llx->0x%llx +0x%llx",
			addr, einfo.addr, einfo.len);

	return mtd_erase(self->mtd, &einfo);
}

static int m__mtd_gl_isbad_mapped(struct mtd_gl_desc* self, u64 addr)
{
	int ret;
	ret = mtd_block_isbad(self->mtd, addr + self->badsize);
	if (ret)
	{
		GL_PRINTF("mtd_gl_isbad BAD: 0x%llx->0x%llx",
				addr, addr + self->badsize);
		self->badsize += self->mtd->erasesize;
	}

	return ret;
}

static int m__mtd_gl_markbad_mapped(struct mtd_gl_desc* self, u64 addr)
{
	int ret;
	GL_PRINTF("mtd_gl_markbad %08llx->%08llx",
			addr, addr + self->badsize);
	ret = mtd_block_markbad(self->mtd, addr + self->badsize);
	self->badsize += self->mtd->erasesize;
	return ret;
}

static void m__mtd_gl_clearbad(struct mtd_gl_desc* self)
{
	self->badsize = 0;
}

static int m__mtd_gl_write_mapped(struct mtd_gl_desc* self,
		u64 addr, u64 size, u64* retsize, void* buf)
{
	long unsigned int ws;
	int64_t i = 0;
	int ret;

	*retsize = 0;

	while(i < size)
	{
		// Align to erase block boundary is Safe
		// Now just assume offset is aligned
		ret = mtd_gl_erase(self, addr + i);
		if (ret < 0)
		{
			ret = mtd_gl_markbad(self, addr + i);
			if (ret < 0)
				return -1;
			continue;
		}

		mtd_write(self->mtd, addr + i + self->badsize,
				self->mtd->erasesize, &ws, buf + i);
		GL_PRINTF("mtd_gl_write 0x%llx->0x%llx +0x%lx",
				addr + i, addr + i + self->badsize, ws);
		*retsize = *retsize + ws;
		i += self->mtd->erasesize;
	}

	return 0;
}

static int m__mtd_gl_read_mapped(struct mtd_gl_desc* self,
		u64 addr, u64 size, u64* retsize, void* buf)
{
	long unsigned int rs;
	int64_t i = 0;
	int ret;

	*retsize = 0;

	while (i < size)
	{
		if (mtd_gl_isbad(self, addr + i))
			continue;

		ret = mtd_read(self->mtd, addr + i + self->badsize,
				self->mtd->erasesize, &rs, buf + i);
		if(ret < 0)
			return -1;
		GL_PRINTF("mtd_gl_read 0x%llx->0x%llx +0x%lx",
				addr + i, addr + i + self->badsize, rs);
		*retsize = *retsize + rs;
		i += self->mtd->erasesize;
	}
	return 0;
}

static u64 m__mtd_gl_remain_mapped(struct mtd_gl_desc* self, u64 oip, int part)
{
	struct mtd_info *mtdpart;
	int i;

	if (part == 0)
		mtdpart = self->mtd;
	else
	{
		i = 1;
		list_for_each_entry(mtdpart, &self->mtd->partitions, node) {
			GL_PRINTF("MTD PARTITION  - 0x%012llx-0x%012llx : \"%s\"\n",
			       mtdpart->offset,
			       mtdpart->offset + mtdpart->size,
			       mtdpart->name);
			if (i == part)
				goto out;
			i++;
		}
		return 0;
	}

out:
	return (u64)(mtdpart->size) - oip - self->badsize;
}

static u64 m__mtd_gl_offset(struct mtd_gl_desc* self, int part)
{
	struct mtd_info *mtdpart;
	int i;

	if (part == 0)
		return 0;

	i = 1;
	list_for_each_entry(mtdpart, &self->mtd->partitions, node) {
		GL_PRINTF("MTD PARTITION  - 0x%012llx-0x%012llx : \"%s\"\n",
		       mtdpart->offset,
		       mtdpart->offset + mtdpart->size,
		       mtdpart->name);
		if (i == part)
			return mtdpart->offset;
		i++;
	}
	return 0;
}

struct mtd_gl_desc* mtd_gl_init(struct mtd_info* mtd)
{
	struct mtd_gl_desc* desc = calloc(1, sizeof(*desc));
	if (desc == NULL)
		return NULL;
	desc->mtd = mtd;
	desc->erase = m__mtd_gl_erase_mapped;
	desc->isbad = m__mtd_gl_isbad_mapped;
	desc->markbad = m__mtd_gl_markbad_mapped;
	desc->clearbad = m__mtd_gl_clearbad;
	desc->write = m__mtd_gl_write_mapped;
	desc->read = m__mtd_gl_read_mapped;
	desc->remain = m__mtd_gl_remain_mapped;
	desc->offset = m__mtd_gl_offset;
	return desc;
}
void mtd_gl_destroy(struct mtd_gl_desc* desc)
{
	if (desc != NULL)
		free(desc);
}

#define HEADER0			'\x1f'
#define HEADER1			'\x8b'
#define	ZALLOC_ALIGNMENT	16
#define HEAD_CRC		2
#define EXTRA_FIELD		4
#define ORIG_NAME		8
#define COMMENT			0x10
#define RESERVED		0xe0
#define DEFLATED		8

int gzwrite_mtd_gl(unsigned char *src, int len,
	    struct mtd_gl_desc* dev,
	    unsigned long szwritebuf,
	    ulong startoffs,
	    ulong szexpected,
	    u64* retsize)
{
	int i, flags;
	z_stream s;
	int r = 0;
	unsigned char *writebuf;
	unsigned crc = 0;
	ulong totalfilled = 0;
	lbaint_t blksperbuf, outblock;
	u32 expected_crc;
	u32 payload_size;
	int iteration = 0;

	if (!szwritebuf ||
	    (szwritebuf % dev->mtd->writesize) ||
	    (szwritebuf < dev->mtd->writesize)) {
		printf("%s: size %lu not a multiple of %u\n",
		       __func__, szwritebuf, dev->mtd->writesize);
		return -1;
	}

	if (startoffs & (dev->mtd->writesize-1)) {
		printf("%s: start offset %lu not a multiple of %u\n",
		       __func__, startoffs, dev->mtd->writesize);
		return -1;
	}

	blksperbuf = szwritebuf / dev->mtd->writesize;
	outblock = lldiv(startoffs, dev->mtd->writesize);

	/* skip header */
	i = 10;
	flags = src[3];
	if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
		puts("Error: Bad gzipped data\n");
		return -1;
	}
	if ((flags & EXTRA_FIELD) != 0)
		i = 12 + src[10] + (src[11] << 8);
	if ((flags & ORIG_NAME) != 0)
		while (src[i++] != 0)
			;
	if ((flags & COMMENT) != 0)
		while (src[i++] != 0)
			;
	if ((flags & HEAD_CRC) != 0)
		i += 2;

	if (i >= len-8) {
		puts("Error: gunzip out of data in header");
		return -1;
	}

	payload_size = len - i - 8;

	memcpy(&expected_crc, src + len - 8, sizeof(expected_crc));
	expected_crc = le32_to_cpu(expected_crc);
	u32 szuncompressed;
	memcpy(&szuncompressed, src + len - 4, sizeof(szuncompressed));
	if (szexpected == 0) {
		szexpected = le32_to_cpu(szuncompressed);
	} else if (szuncompressed != (u32)szexpected) {
		printf("size of %lx doesn't match trailer low bits %x\n",
		       szexpected, szuncompressed);
		return -1;
	}
	if (lldiv(szexpected, dev->mtd->writesize) > (dev->mtd->size - outblock)) {
		printf("%s: uncompressed size %lu exceeds device size\n",
		       __func__, szexpected);
		return -1;
	}

	gzwrite_progress_init(szexpected);

	s.zalloc = gzalloc;
	s.zfree = gzfree;

	r = inflateInit2(&s, -MAX_WBITS);
	if (r != Z_OK) {
		printf("Error: inflateInit2() returned %d\n", r);
		return -1;
	}

	s.next_in = src + i;
	s.avail_in = payload_size+8;
	writebuf = (unsigned char *)malloc_cache_aligned(szwritebuf);

	/* decompress until deflate stream ends or end of file */
	do {
		if (s.avail_in == 0) {
			printf("%s: weird termination with result %d\n",
			       __func__, r);
			break;
		}

		/* run inflate() on input until output buffer not full */
		do {
			unsigned long blocks_written;
			int numfilled;
			lbaint_t writeblocks;
			u64 ws;

			s.avail_out = szwritebuf;
			s.next_out = writebuf;
			r = inflate(&s, Z_SYNC_FLUSH);
			if ((r != Z_OK) &&
			    (r != Z_STREAM_END)) {
				printf("Error: inflate() returned %d\n", r);
				goto out;
			}
			numfilled = szwritebuf - s.avail_out;
			crc = crc32(crc, writebuf, numfilled);
			totalfilled += numfilled;
			if (numfilled < szwritebuf) {
				writeblocks = (numfilled+dev->mtd->writesize-1)
						/ dev->mtd->writesize;
				memset(writebuf+numfilled, 0,
				       dev->mtd->writesize-(numfilled%dev->mtd->writesize));
			} else {
				writeblocks = blksperbuf;
			}

			gzwrite_progress(iteration++,
					 totalfilled,
					 szexpected);
			mtd_gl_write(dev, outblock*dev->mtd->writesize,
					writeblocks*dev->mtd->writesize,
					&ws, writebuf);
			blocks_written = ws/dev->mtd->writesize;
			outblock += blocks_written;
			if (ctrlc()) {
				puts("abort\n");
				goto out;
			}
			WATCHDOG_RESET();
		} while (s.avail_out == 0);
		/* done when inflate() says it's done */
	} while (r != Z_STREAM_END);

	if ((szexpected != totalfilled) ||
	    (crc != expected_crc))
		r = -1;
	else
		r = 0;

out:
	*retsize = szexpected;
	gzwrite_progress_finish(r, totalfilled, szexpected,
				expected_crc, crc);
	free(writebuf);
	inflateEnd(&s);

	return r;
}


