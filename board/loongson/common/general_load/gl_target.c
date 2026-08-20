#include <mmc.h>
#include <scsi.h>
#include <nand.h>
#include <blk.h>

#include "gl_target.h"
#include "device/mtd.h"
#include "device/net.h"
#include "gl_debug.h"

typedef struct gl_target_priv_s {
	int type;
	int part;
	void* desc;
	int fmt;
	char symbol[32];
} gl_target_priv;

static int gl_atoi(char* s, char** end)
{
	char* c;
	int i;

	for (c = s; *c != 0x00; c++)
	{
		if (*c < '0' || *c > '9')
			break;
		i = i * 10 + (*c - '0');
	}

	if (end != NULL)
		*end = c;

	return i;
}
// 0,1 = gl_get_devnum(0:1)
static int gl_get_devnum(char* if_part, int* part)
{
	char* p;
	int idx;

	if (if_part == NULL)
		return 0;

	idx = gl_atoi(if_part, &p);

	if (p != NULL && *p == ':')
		*part = gl_atoi(p + 1, NULL);

	GL_PRINTF("%s: [%d,%d]", if_part, idx, *part);

	return idx;
}

static void* gl_ram_init(void)
{
	return (void*)0xff;
}

static struct net_gl_desc* gl_net_init(char* ip)
{
	return net_gl_init(ip);
}

static struct blk_desc* gl_mmc_init(int devnum)
{
	return blk_get_devnum_by_type(IF_TYPE_MMC, devnum);
}

static struct blk_desc* gl_usb_init(int devnum)
{
	return blk_get_devnum_by_type(IF_TYPE_USB, devnum);
}

static struct blk_desc* gl_scsi_init(int devnum)
{
#ifdef CONFIG_SCSI
	int ret;
#ifndef CONFIG_DM_SCSI
	scsi_bus_reset(NULL);
#endif
	ret = scsi_scan(true);
	if (ret)
		return NULL;
#endif
	return blk_get_devnum_by_type(IF_TYPE_SCSI, devnum);
}

static struct mtd_gl_desc* gl_nand_init(int devnum)
{
#ifdef CONFIG_MTD_RAW_NAND
	struct mtd_gl_desc* desc;
	struct mtd_info* mtd;

	mtd_probe_devices();
	mtd = get_nand_dev_by_index(devnum);
	desc = mtd_gl_init(mtd);

	return desc;
#else
	return 0;
#endif
}

static struct mtd_gl_desc* gl_flash_init(int devnum)
{
	struct mtd_gl_desc* desc;
	struct mtd_info *mtd;
	int i = 0;

	mtd_probe_devices();

	mtd_for_each_device(mtd) {
		if (mtd->type == MTD_NORFLASH) {
			if (i == devnum)
				break;
			i++;
		}
	}

	desc = mtd_gl_init(mtd);

	return desc;
}

static int m__target_set_device(gl_target_t* self, char* if_part)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;

	if (priv->type != GL_DEVICE_NONE || priv->desc != NULL)
	{
		printf("Taget Device Already Set\n");
		return -1;
	}

	if (strncmp(if_part, "ram", 3) == 0) {
		priv->type = GL_DEVICE_RAM;
		priv->desc = gl_ram_init();
	}
	else if (strncmp(if_part, "net", 3) == 0) {
		priv->type = GL_DEVICE_NET;
		priv->desc = gl_net_init(if_part + 4);
	}
	else if (strncmp(if_part, "mmc", 3) == 0) {
		priv->type = GL_DEVICE_BLK;
		priv->desc = gl_mmc_init(
				gl_get_devnum(if_part + 3, &priv->part));
	}
	else if (strncmp(if_part, "usb", 3) == 0) {
		priv->type = GL_DEVICE_BLK;
		priv->desc = gl_usb_init(
				gl_get_devnum(if_part + 3, &priv->part));
	}
	else if (strncmp(if_part, "scsi", 4) == 0) {
		priv->type = GL_DEVICE_BLK;
		priv->desc = gl_scsi_init(
				gl_get_devnum(if_part + 4, &priv->part));
	}
	else if	(strncmp(if_part, "nand", 4) == 0) {
		priv->type = GL_DEVICE_MTD;
		priv->desc = gl_nand_init(
				gl_get_devnum(if_part + 4, &priv->part));
	}
	else if	(strncmp(if_part, "flash", 5) == 0) {
		priv->type = GL_DEVICE_MTD;
		priv->desc = gl_flash_init(
				gl_get_devnum(if_part + 5, &priv->part));
	}
	else
	{
		printf("Taget Set Device Error: interface unknown %s\n", if_part);
		return -1;
	}

	if (priv->desc == NULL)
		return -1;

	return 0;
}

static int m__target_get_type(gl_target_t* self)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;
	return priv->type;
}
static int m__target_get_part(gl_target_t* self)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;
	return priv->part;
}
static void* m__target_get_desc(gl_target_t* self)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;
	return priv->desc;
}
static void m__target_set_fmt(gl_target_t* self, int fmt)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;
	priv->fmt = fmt;
}
static int m__target_get_fmt(gl_target_t* self)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;
	return priv->fmt;
}
static void m__target_set_symbol(gl_target_t* self, char* symbol)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;
	snprintf(priv->symbol, sizeof(priv->symbol), "%s", symbol);
}
static char* m__target_get_symbol(gl_target_t* self)
{
	gl_target_priv* priv = (gl_target_priv*)self->priv;
	return priv->symbol;
}

gl_target_t* new_gl_target(void)
{
	gl_target_t* target = calloc(1,
			sizeof(gl_target_t) + sizeof(gl_target_priv));
	if (target == NULL)
		return NULL;

	target->priv = target + 1;
	target->set_device = m__target_set_device;
	target->get_type = m__target_get_type;
	target->get_part = m__target_get_part;
	target->get_desc = m__target_get_desc;
	target->set_fmt = m__target_set_fmt;
	target->get_fmt = m__target_get_fmt;
	target->set_symbol = m__target_set_symbol;
	target->get_symbol = m__target_get_symbol;

	return target;
}

void destroy_gl_target(gl_target_t* target)
{
	int gl_device;
	if (target == NULL)
		return;

	gl_device = gl_target_get_type(target);
	if (gl_device == GL_DEVICE_MTD) {
		struct mtd_gl_desc* desc = gl_target_get_desc(target);
		mtd_gl_destroy(desc);
	} else if (gl_device == GL_DEVICE_NET) {
		struct net_gl_desc* desc = gl_target_get_desc(target);
		net_gl_destroy(desc);
	}

	free(target);
}


