#ifndef __GENERAL_LOAD_MTD_H__
#define __GENERAL_LOAD_MTD_H__

#include <mtd.h>

#define mtd_gl_erase(d, e) d->erase(d, e)
#define mtd_gl_isbad(d, a) d->isbad(d, a)
#define mtd_gl_markbad(d, a) d->markbad(d, a)
#define mtd_gl_clearbad(d) d->clearbad(d)
#define mtd_gl_write(d, a, s, r, b) d->write(d, a, s, r, b)
#define mtd_gl_read(d, a, s, r, b) d->read(d, a, s, r, b)
#define mtd_gl_partition_remain(d, o, p) d->remain(d, o, p)
#define mtd_gl_partition_offset(d, p) d->offset(d, p)

struct mtd_gl_desc {
	struct mtd_info* mtd;
	u64 badsize;
	int (*erase)(struct mtd_gl_desc* self, u64 addr);
	int (*isbad)(struct mtd_gl_desc* self, u64 addr);
	int (*markbad)(struct mtd_gl_desc* self, u64 addr);
	void (*clearbad)(struct mtd_gl_desc* self);
	int (*write)(struct mtd_gl_desc* self,
			u64 addr, u64 size, u64* retsize, void* buf);
	int (*read)(struct mtd_gl_desc* self,
			u64 addr, u64 size, u64* retsize, void* buf);
	u64 (*remain)(struct mtd_gl_desc* self, u64 oip, int part);
	u64 (*offset)(struct mtd_gl_desc* self, int part);
};

struct mtd_gl_desc* mtd_gl_init(struct mtd_info* mtd);
void mtd_gl_destroy(struct mtd_gl_desc* desc);

int gzwrite_mtd_gl(unsigned char *src, int len,
	    struct mtd_gl_desc* dev,
	    unsigned long szwritebuf,
	    ulong startoffs,
	    ulong szexpected,
	    u64* retsize);

#endif
