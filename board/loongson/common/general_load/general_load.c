#include <image.h>
#include <lmb.h>
#include <mmc.h>
#include <scsi.h>
#include <nand.h>
#include <mtd.h>
#include <blk.h>
#include <part.h>
#include <gzip.h>
#include <mach/addrspace.h>
#include <asm/addrspace.h>
#include "general_load.h"
#include "device/mtd.h"
#include "device/net.h"
#include "utils/flash_secure.h"
#include "gl_debug.h"

#define ldbr_get_ld_type(ldbr) \
	gl_target_get_type(ldbr->src)
#define ldbr_get_br_type(ldbr) \
	gl_target_get_type(ldbr->dest)
#define ldbr_get_ld_part(ldbr) \
	gl_target_get_part(ldbr->src)
#define ldbr_get_br_part(ldbr) \
	gl_target_get_part(ldbr->dest)
#define ldbr_get_ld_desc(ldbr) \
	gl_target_get_desc(ldbr->src)
#define ldbr_get_br_desc(ldbr) \
	gl_target_get_desc(ldbr->dest)
#define ldbr_get_ld_fmt(ldbr) \
	gl_target_get_fmt(ldbr->src)
#define ldbr_get_br_fmt(ldbr) \
	gl_target_get_fmt(ldbr->dest)
#define ldbr_get_ld_symbol(ldbr) \
	gl_target_get_symbol(ldbr->src)
#define ldbr_get_br_symbol(ldbr) \
	gl_target_get_symbol(ldbr->dest)

typedef struct ldbr_s ldbr_t;

struct ldbr_s {
	gl_target_t* src;
	gl_target_t* dest;
	bool once_only;
	int(*ld)(ldbr_t* self, u64 offset, void* buf, u64 size, u64* retsize);
	int(*ldfin)(ldbr_t* self);
	int(*br)(ldbr_t* self, u64 offset, void* buf, u64 size, u64* retsize);
	int(*brfin)(ldbr_t* self, u64 offset);
};

static int64_t ldbr_blk_partition_offset(struct blk_desc* desc, int part)
{
	int p;
	struct disk_partition part_info;

	if (part == 0)
		return 0;

	p = part_get_info(desc, part, &part_info);
	if (p < 0)
		return 0;

	GL_PRINTF("partition offset [%d] offset = %d",
			part, (int)part_info.start);

	return part_info.start * part_info.blksz;
}

static int m__ld_net(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct net_gl_desc* desc = ldbr_get_ld_desc(self);
	char* filepath = ldbr_get_ld_symbol(self);
	int proto = ldbr_get_ld_fmt(self);

	return net_gl_read(desc, proto, filepath, size, retsize, buf);
}

static int m__ld_ram(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	*retsize = size;
	return 0;
}

static int m__br_ram(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	*retsize = size;
	return 0;
}

static int m__ld_blk(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct blk_desc* desc = ldbr_get_ld_desc(self);
	int part = ldbr_get_ld_part(self);
	u32 blk, cnt, n;

	offset += ldbr_blk_partition_offset(desc, part);
	blk = offset/desc->blksz;
	cnt = size/desc->blksz + 1;
	n = blk_dread(desc, blk, cnt, buf);
	*retsize = n * desc->blksz;

	return 0;
}

static int m__br_blk(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct blk_desc* desc = ldbr_get_br_desc(self);
	int part = ldbr_get_br_part(self);
	u32 blk, cnt, n;

	offset += ldbr_blk_partition_offset(desc, part);
	blk = offset/desc->blksz;
	cnt = size/desc->blksz + 1;
	n = blk_dwrite(desc, blk, cnt, buf);
	*retsize = n * desc->blksz;

	return 0;
}

static int m__ld_blk_fs(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct blk_desc* desc = ldbr_get_ld_desc(self);
	int part = ldbr_get_ld_part(self);
	char* filepath = ldbr_get_ld_symbol(self);
	int ret;

	if (fs_set_blk_dev_with_part(desc, part) == 0)
	{
		ret = fs_read(filepath, (ulong)buf, offset, size, retsize);
		if (ret == -ENODATA)
		{
			*retsize = 0;
			return 0;
		}
		else
		{
			return ret;
		}
	}
	return 0;
}

static int m__br_blk_fs(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct blk_desc* desc = ldbr_get_br_desc(self);
	int part = ldbr_get_br_part(self);
	char* filepath = ldbr_get_br_symbol(self);

	if (fs_set_blk_dev_with_part(desc, part) == 0)
	{
		if (fs_write(filepath, (ulong)buf, offset, size, retsize) < 0)
			return -1;
	}
	return 0;
}

static int m__br_blk_decompress(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct blk_desc* desc = ldbr_get_br_desc(self);
	int part = ldbr_get_br_part(self);

	offset += ldbr_blk_partition_offset(desc, part);
	if (gzwrite(buf, size, desc, 1024*1024, offset, 0)
					== 0)
		*retsize = size;
	return 0;
}

static int m__ld_mtd(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct mtd_gl_desc* desc = ldbr_get_ld_desc(self);
	int part = ldbr_get_ld_part(self);
	u64 remain;

	remain = mtd_gl_partition_remain(desc, offset, part);
	offset += mtd_gl_partition_offset(desc, part);

	return mtd_gl_read(desc, offset,
			size < remain ? size : remain, retsize, buf);
}

static int m__br_mtd(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct mtd_gl_desc* desc = ldbr_get_br_desc(self);
	int part = ldbr_get_br_part(self);
	u64 remain;

	remain = mtd_gl_partition_remain(desc, offset, part);
	offset += mtd_gl_partition_offset(desc, part);

	return mtd_gl_write(desc, offset,
			size < remain ? size : remain, retsize, buf);
}

static int m__br_mtd_decompress(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct mtd_gl_desc* desc = ldbr_get_br_desc(self);
	int part = ldbr_get_br_part(self);
	u64 remain;

	remain = mtd_gl_partition_remain(desc, offset, part);
	offset += mtd_gl_partition_offset(desc, part);


	return gzwrite_mtd_gl(buf, size, desc, 1024*1024, offset, 0, retsize);
}

static int m__br_mtd_ubootsecure(ldbr_t* self, u64 offset,
		void* buf, u64 size, u64* retsize)
{
	struct mtd_gl_desc* desc = ldbr_get_br_desc(self);
	int part = ldbr_get_br_part(self);
	u64 remain;

	if (!uboot_secure(buf, size))
		return -1;

	remain = mtd_gl_partition_remain(desc, offset, part);
	offset += mtd_gl_partition_offset(desc, part);

	return mtd_gl_write(desc, offset,
			size < remain ? size : remain, retsize, buf);
}

static int m__ldfin_mtd_clearbad(ldbr_t* self)
{
	struct mtd_gl_desc* desc = ldbr_get_ld_desc(self);

	mtd_gl_clearbad(desc);

	return 0;
}

static int m__brfin_mtd_eraserest(ldbr_t* self, u64 offset)
{
	struct mtd_gl_desc* desc = ldbr_get_br_desc(self);
	int part = ldbr_get_br_part(self);
	int64_t i = 0;
	int ret;
	u64 remain;

	remain = mtd_gl_partition_remain(desc, offset, part);
	offset += mtd_gl_partition_offset(desc, part);

	/* Erase -> Mark */
	while(i < remain)
	{
		// Align to erase block boundary is Safe
		// Now just assume offset is aligned
		ret = mtd_gl_erase(desc, offset + i);
		if (ret < 0) {
			ret = mtd_gl_markbad(desc, offset + i);
			if (ret < 0){
				return -1;
			}
		}
		i += desc->mtd->erasesize;
	}

	mtd_gl_clearbad(desc);

	return 0;
}

static int m__ldfin_none(ldbr_t* self)
{
	return 0;
}
static int m__brfin_none(ldbr_t* self, u64 offset)
{
	return 0;
}

static int ldbr_run(ldbr_t* ldbr)
{
	void* buf = (void*)CONFIG_SYS_LOAD_ADDR;
	u64 buf_size;
#ifdef CONFIG_LMB
	struct lmb lmb;
	lmb_init_and_reserve(&lmb, gd->bd, (void *)gd->fdt_blob);
	buf_size = lmb_get_free_size(&lmb, image_load_addr);
#else
	//buf_size = 0x24000000; // 576M (512M+64M)
	//buf_size = 0xA000000; //160M (128M+32M)
	buf_size = 0x1000000; // 16M
#endif
	u64 offset = 0;
	u64 ldsize = 0;
	u64 brsize = 0;

	while (1) {
		printf("loading...\n");
		if (ldbr->ld(ldbr, offset, buf, buf_size, &ldsize))
			return -1;
		if (ldsize == 0)
			break;
		printf("loaded&burning %lld bytes ...\n", ldsize);
		if (ldbr->br(ldbr, offset, buf, ldsize, &brsize))
			return -1;
		if (brsize == 0)
			break;
		offset += brsize;
		printf("load&burn %lld finished\n", offset);
		if (ldbr->once_only)
			break;
	}

	ldbr->ldfin(ldbr);
	ldbr->brfin(ldbr, offset);

	return 0;
}

static int ldbr_init_ld(ldbr_t* ldbr)
{
	int fstype = ldbr_get_ld_fmt(ldbr);
	enum gl_device_e gl_device = ldbr_get_ld_type(ldbr);

	switch (gl_device)
	{
		case GL_DEVICE_RAM:
			ldbr->ld = m__ld_ram;
			break;
		case GL_DEVICE_NET:
			ldbr->ld = m__ld_net;
			break;
		case GL_DEVICE_BLK:
			if (fstype == FS_TYPE_ANY)
				ldbr->ld = m__ld_blk;
			else
				ldbr->ld = m__ld_blk_fs;
			break;
		case GL_DEVICE_MTD:
			ldbr->ld = m__ld_mtd;
			ldbr->ldfin = m__ldfin_mtd_clearbad;
			break;
		default:
			GL_PRINTF("ERR Device Type %d", gl_device);
			return -1;
	}
	return 0;
}

static int ldbr_init_br(ldbr_t* ldbr, enum gl_extra_e extra)
{
	int fstype = ldbr_get_br_fmt(ldbr);
	enum gl_device_e gl_device = ldbr_get_br_type(ldbr);

	switch (gl_device)
	{
		case GL_DEVICE_RAM:
			ldbr->br = m__br_ram;
			break;
		case GL_DEVICE_BLK:
			if (fstype == FS_TYPE_ANY)
				if (extra & GL_EXTRA_DECOMPRESS)
					ldbr->br = m__br_blk_decompress;
				else
					ldbr->br = m__br_blk;
			else
				ldbr->br = m__br_blk_fs;
			break;
		case GL_DEVICE_MTD:
			// extra 暂时多选一
			if (extra & GL_EXTRA_DECOMPRESS)
				ldbr->br = m__br_mtd_decompress;
			else if (extra & GL_EXTRA_UBOOTSECURE)
				ldbr->br = m__br_mtd_ubootsecure;
			else
				ldbr->br = m__br_mtd;
			ldbr->brfin = m__brfin_mtd_eraserest;
			break;
		default:
			GL_PRINTF("ERR Device Type %d", gl_device);
			return -1;
	}

	return 0;
}

static int ldbr_init(ldbr_t* ldbr,
		gl_target_t* src, gl_target_t* dest, enum gl_extra_e extra)
{
	ldbr->src = src;
	ldbr->dest = dest;

	if (extra != GL_EXTRA_NONE)
		ldbr->once_only = true;
	if (gl_target_get_type(src) == GL_DEVICE_NET ||
			gl_target_get_type(src) == GL_DEVICE_RAM ||
			gl_target_get_type(dest) == GL_DEVICE_RAM)
		ldbr->once_only = true;

	if (ldbr_init_ld(ldbr))
		return -1;

	if (ldbr_init_br(ldbr, extra))
		return -1;

	return 0;
}

/*
 * OLD:
 * 	GL = LD.BR = ld.br...ld.br
 *
 * 	缺陷1：加入解压、安全校验操作后无法分解，流程处理别扭
 * 	解压、安全校验 记作 EX
 * 	GL = LD.EX.BR = ld.ex.br != ld.ex.br...ld.ex.br
 * 	因为解压与安全校验无法分解成小段进行，必须获取全部数据后才能进行
 *
 * 	缺陷2：对于NAND要擦除分区剩余空间难以处理
 * 	NAND擦除分区剩余空间记作 ER
 * 	之前没有设计单独的结束处理操作，造成本该是以下结构：
 * 	GL = LD.BR.ER = ld.br...ld.br.er
 * 	不得不将 BR 与 ER 结合起来 BR-ER
 * 	GL = LD.BR-ER = ld.br-er...ld.br-er
 * 	结果当GL可以拆成多段 ld.br-er 时，er 会被执行多次
 *
 * 	上面两个缺陷都是在拆解成多段后才暴露的
 * 	归结原因是在开始设计 GL 时没考虑到分解情况
 *
 * NEW:
 *	GL = LD.BR.FIN = ld.br...ld.br.ldfin.brfin
 *
 *	其中 EX 作为 BR 的修饰可以合为一体，而且加入分解控制（once-only）
 *	GL = LD.EX-BR.FIN = ld.ex-br.ldfin.brfin
 *	NAND擦除剩余空间可在 FIN 中处理
 * */
int general_load(gl_target_t* src, gl_target_t* dest, enum gl_extra_e extra)
{
	ldbr_t ldbr = {
		.ldfin = m__ldfin_none,
		.brfin = m__brfin_none,
	};
	if (ldbr_init(&ldbr, src, dest, extra))
		return -1;
	ldbr_run(&ldbr);
	return 0;
}

