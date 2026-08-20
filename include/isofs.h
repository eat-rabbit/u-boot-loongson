#ifndef _ISOFS_H_
#define _ISOFS_H_

#include <part_iso.h>
#include <part.h>

int isofs_probe(struct blk_desc *dev_desc, struct disk_partition *info);
int isofs_read_file(const char *path, void *buf, loff_t offset, loff_t len,
		  loff_t *actread);
int isofs_size(const char *path, loff_t *size);
int isofs_ls(const char *path);
void isofs_close(void);
#endif /* _ISOFS_H_ */
