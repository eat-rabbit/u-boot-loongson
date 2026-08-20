#include <common.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <edid.h>
#include <log.h>
#include <video_bridge.h>
#include <linux/delay.h>

typedef struct ms1824_cfg_s {
	uint16_t reg;
	uint8_t val;
	bool delay;
} ms1824_cfg_t;

typedef struct ms1824_cfg_table_s
{
	char name[16];
	ms1824_cfg_t* cfg;
	long int len;
} ms1824_cfg_table_t;

#include "ms1824_cfg_800x480.c"
#include "ms1824_cfg_480x272.c"

static ms1824_cfg_t* MS1824_CFG;
static long int MS1824_CFG_LEN;

static ms1824_cfg_table_t MS1824_CFG_TABLE [] =
{
	{"480x272", (ms1824_cfg_t*)MS1824_480x272_CFG, ARRAY_SIZE(MS1824_480x272_CFG)},
	{"800x480", (ms1824_cfg_t*)MS1824_800x480_CFG, ARRAY_SIZE(MS1824_800x480_CFG)},
};

static int ms1824_reset(struct udevice* dev)
{
	struct gpio_desc reset_gpio;
	int ret;
	ret = gpio_request_by_name(dev, "gpios", 0, &reset_gpio,
				   GPIOD_IS_OUT);
	if (ret)
	{
		printf("Error: MS1824 cannot get reset GPIO (err=%d)\n", ret);
		return 0;
	}
	dm_gpio_set_value(&reset_gpio, 1);
	udelay(10000);
	dm_gpio_set_value(&reset_gpio, 0);
	udelay(10000);
	dm_gpio_set_value(&reset_gpio, 1);
	udelay(10000);
	dm_gpio_free(dev, &reset_gpio);
	return 0;
}

static int ms1824_get_cfg(char* resolution)
{
	int i = 0;

//	printf ("cfg = %p:%ld,%p:%ld | table = %p:%ld,%p:%ld\n",
//			MS1824_480x272_CFG, ARRAY_SIZE(MS1824_480x272_CFG),
//			MS1824_800x480_CFG, ARRAY_SIZE(MS1824_800x480_CFG),
//			MS1824_CFG_TABLE[0].cfg, MS1824_CFG_TABLE[0].len,
//			MS1824_CFG_TABLE[1].cfg, MS1824_CFG_TABLE[1].len);
	for (i = 0; i < ARRAY_SIZE(MS1824_CFG_TABLE); i++)
	{
		if (strcmp(resolution, MS1824_CFG_TABLE[i].name) == 0)
		{
			printf("MS1824 use resolution=%s\n", 
					MS1824_CFG_TABLE[i].name);
			MS1824_CFG = MS1824_CFG_TABLE[i].cfg;
			MS1824_CFG_LEN = MS1824_CFG_TABLE[i].len;
			return 0;
		}
	}
//	printf("ERROR-MS1824: Invalid Resolution %s\n",
//			resolution);
	return -1;
}

static int ms1824_probe(struct udevice* dev)
{
	int i = 0;
//	uint8_t val;
	uint16_t reg;
	char* resolution;
	int ret = 0;

	i2c_set_chip_offset_len(dev, 2);

	ms1824_reset(dev);

	resolution = (char*)dev_read_prop(dev, "resolution", NULL);
	if (resolution == NULL)
	{
		printf("ERROR-MS1824: Get Resolution fail!\n");
		return -1;
	}

	if (ms1824_get_cfg(resolution) != 0)
		return -1;

//	printf("MS1824: Get config %p:%d\n", MS1824_CFG, MS1824_CFG_LEN);
	for (i = 0; i < MS1824_CFG_LEN; i++)
	{
		reg = ((MS1824_CFG[i].reg & 0xff) << 8) |
			(MS1824_CFG[i].reg >> 8);
		ret = dm_i2c_write(dev, reg, &(MS1824_CFG[i].val), 1);
		if (ret != 0)
		{
			printf ("================MS1824 i2c write error!==================\n");
			return -1;
		}

		if (MS1824_CFG[i].delay)
			udelay(1000);
//		dm_i2c_read(dev, reg, &val, 1);
//		if (val != MS1824_CFG[i].val)
//		{
//			printf("0x%04x: 0x%02x<-->0x%02x\n",
//				reg, MS1824_CFG[i].val, val);
//		}
	}

	printf ("MS1824 Probed finish\n");
	return 0;
}

static const struct udevice_id ms1824_ids[] = {
	{ .compatible = "macrosilicon,ms1824", },
	{ }
};

U_BOOT_DRIVER(macrosilicon_ms1824) = {
	.name	= "macrosilicon_1824",
	.id	= UCLASS_VIDEO_BRIDGE,
	.of_match = ms1824_ids,
	.probe	= ms1824_probe,
};

