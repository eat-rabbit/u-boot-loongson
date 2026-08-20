/*
 * 将 MCP4728 I2C 地址从 0x60 -> mcp4728,i2c-addr
 * */
#include <stdlib.h>
#include <common.h>
#include <errno.h>
#include <dm.h>
#include <asm/gpio.h>
#include <linux/delay.h>

#define I2C_DELAY_USEC 10

struct mcp4728 {
	int *sda;
	int *scl;
	int ldac;
	// A2|A1|A0
	// origin id
	u8 id;
	// new i2c addr
	u8 addr;
};

struct mcp4728_i2cid_priv {
	int sda;
	int scl;
	int num;
	struct mcp4728* mcp4728;
};

static void soft_i2c_idle(struct mcp4728* mcp4728)
{
	gpio_direction_output(*(mcp4728->scl), 1);
	gpio_direction_output(*(mcp4728->sda), 1);
	gpio_direction_output(mcp4728->ldac, 1);
}

static int priv_mcp4728_init(struct mcp4728_i2cid_priv* priv)
{
	struct mcp4728* mcp4728;
	if (gpio_request(priv->sda, "mcp4728-sda")) {
		pr_err("mcp4728-sda fail\n");
		return -1;
	}
	if (gpio_request(priv->scl, "mcp4728-scl")) {
		pr_err("mcp4728-scl fail\n");
		return -1;
	}

	for (mcp4728 = priv->mcp4728;
			mcp4728 < priv->mcp4728 + priv->num;
			mcp4728++)
	{
		if (gpio_request(mcp4728->ldac, "mcp4728-ldac")) {
			pr_err("mcp4728-ldac fail\n");
			return -1;
		}

		soft_i2c_idle(mcp4728);
	}

	return 0;
}

static u8 soft_i2c_read(struct mcp4728* mcp4728, u8 last)
{
	u8 b = 0;
	u8 i;

	// make sure pullup enabled
	gpio_set_value(*(mcp4728->sda), 1);
	gpio_direction_input(*(mcp4728->sda));

	// read byte
	for (i = 0; i < 8; i++)
	{
		b <<= 1;
		gpio_set_value(*(mcp4728->scl), 1);
		udelay(I2C_DELAY_USEC);
		if (gpio_get_value(*(mcp4728->sda))) b |= 1;
		gpio_set_value(*(mcp4728->scl), 0);
		udelay(I2C_DELAY_USEC);
	}

	// send Ack or Nak
	gpio_direction_output(*(mcp4728->sda), last);
	gpio_set_value(*(mcp4728->scl), 1);
	udelay(I2C_DELAY_USEC);
	gpio_set_value(*(mcp4728->scl), 0);
	udelay(I2C_DELAY_USEC);
	gpio_set_value(*(mcp4728->sda), 1);

	return b;
}

static u8 soft_i2c_write(struct mcp4728* mcp4728, u8 b)
{
	u8 m;

	// write byte
	for (m = 0x80; m != 0; m >>= 1)
	{
		gpio_set_value(*(mcp4728->sda), !!(m & b));
		gpio_set_value(*(mcp4728->scl), 1);
		udelay(I2C_DELAY_USEC);
		gpio_set_value(*(mcp4728->scl), 0);
		udelay(I2C_DELAY_USEC);
	}

	// get Ack or Nak
	gpio_direction_input(*(mcp4728->sda));
	gpio_set_value(*(mcp4728->scl), 1);
	b = gpio_get_value(*(mcp4728->sda));
	udelay(I2C_DELAY_USEC);
	gpio_set_value(*(mcp4728->scl), 0);
	gpio_direction_output(*(mcp4728->sda), 1);
	udelay(I2C_DELAY_USEC);
	return b == 0;
}

static u8 soft_i2c_ldacwrite(struct mcp4728* mcp4728, u8 b)
{
	u8 m;

	// write byte
	for (m = 0x80; m != 0; m >>= 1)
	{
		gpio_set_value(*(mcp4728->sda), !!(m & b));
		gpio_set_value(*(mcp4728->scl), 1);
		udelay(I2C_DELAY_USEC);
		gpio_set_value(*(mcp4728->scl), 0);
		udelay(I2C_DELAY_USEC);
	}

	// get Ack or Nak
	gpio_set_value(mcp4728->ldac, 0);
	gpio_direction_input(*(mcp4728->sda));
	gpio_set_value(*(mcp4728->scl), 1);
	b = gpio_get_value(*(mcp4728->sda));
	udelay(I2C_DELAY_USEC);
	gpio_set_value(*(mcp4728->scl), 0);
	gpio_direction_output(*(mcp4728->sda), 1);
	udelay(I2C_DELAY_USEC);
	return b == 0;
}

static u8 soft_i2c_start(struct mcp4728* mcp4728, u8 addressRW)
{
	gpio_set_value(*(mcp4728->sda), 0);
	udelay(I2C_DELAY_USEC);
	gpio_set_value(*(mcp4728->scl), 0);
	return soft_i2c_write(mcp4728, addressRW);
}

static u8 soft_i2c_restart(struct mcp4728* mcp4728, u8 addressRW)
{
	gpio_set_value(*(mcp4728->scl), 1);
	return soft_i2c_start(mcp4728, addressRW);
}

static void soft_i2c_stop(struct mcp4728* mcp4728)
{
	gpio_set_value(*(mcp4728->sda), 0);
	udelay(I2C_DELAY_USEC);
	gpio_set_value(*(mcp4728->scl), 1);
	udelay(I2C_DELAY_USEC);
	gpio_set_value(*(mcp4728->sda), 1);
	udelay(I2C_DELAY_USEC);
}

static u8 mcp4728_read_address(struct mcp4728* mcp4728)
{
	u8 ack1;
	u8 ack2;
	u8 ack3;
	u8 address;

	gpio_set_value(mcp4728->ldac, 1);
	ack1 = soft_i2c_start(mcp4728, 0x00);
	ack2 = soft_i2c_ldacwrite(mcp4728, 0x0c);
	ack3 = soft_i2c_restart(mcp4728, 0xc1);
	address = soft_i2c_read(mcp4728, 1);
	soft_i2c_stop(mcp4728);
	udelay(100);
	gpio_set_value(mcp4728->ldac, 1);
	printf("%s: ack1[0x%x], ack2[0x%x], ack3[0x%x], A2A1A0|1|A2A1A0[0x%x]\n",
			__func__, ack1, ack2, ack3, address);

	return address;
}

static void mcp4728_write_address(struct mcp4728* mcp4728, u8 id)
{
	u8 ack1;
	u8 ack2;
	u8 ack3;
	u8 ack4;

	gpio_set_value(mcp4728->ldac, 1);
	ack1 = soft_i2c_start(mcp4728, 0xc0 | (mcp4728->id << 1));
	ack2 = soft_i2c_ldacwrite(mcp4728, 0x61 | (mcp4728->id << 2));
	ack3 = soft_i2c_write(mcp4728, 0x62 | (id << 2));
	ack4 = soft_i2c_write(mcp4728, 0x63 | (id << 2));
	soft_i2c_stop(mcp4728);
	udelay(100);

	printf("%s: ack1[0x%x], ack2[0x%x], ack3[0x%x] ack4[0x%x]\n",
			__func__, ack1, ack2, ack3, ack4);
}

static int drv_mcp4728_i2cid_probe(struct udevice *dev)
{
	struct mcp4728_i2cid_priv* priv = dev_get_priv(dev);
	struct mcp4728* mcp4728;
	u8 id;

	priv_mcp4728_init(priv);

	for (mcp4728 = priv->mcp4728;
			mcp4728 < priv->mcp4728 + priv->num;
			mcp4728++)
	{
		// c0|A2|A1|A0
		id = mcp4728->addr & 0x07;

		// [A2|A1|A0|1|A2|A0|A1|0]
		mcp4728->id = mcp4728_read_address(mcp4728) >> 5;

		printf("probing mcp4728 ids: sda=%d, scl=%d, ldac=%d, id=%d, newid=%d\n",
				*(mcp4728->sda), *(mcp4728->scl), mcp4728->ldac,
				mcp4728->id, id);

		if (mcp4728->id != id)
			mcp4728_write_address(mcp4728, id);

		soft_i2c_idle(mcp4728);
	}

	free(priv->mcp4728);

	return 0;
}

static int drv_mcp4728_i2cid_of_to_plat(struct udevice *dev)
{
	struct mcp4728_i2cid_priv* priv = dev_get_priv(dev);
	int i;
	u32 addr;

	/**
	 * gpio_request_by_name(dev, "sda-gpios", ...)
	 * 还有 BUG，将 PB2 PB3 PB28 的 GPIO 号识别为 2 3 28，
	 * 应该还要加上PB的偏移，
	 * 先在dts中不写成 sda-gpios = <&pio2 2 0> 这种形式
	 * 直接指定GPIO引脚号
	 */
	dev_read_s32(dev, "sda", &(priv->sda));
	dev_read_s32(dev, "scl", &(priv->scl));
	dev_read_s32(dev, "mcp4728,number", &(priv->num));

	priv->mcp4728 = calloc(priv->num, sizeof(struct mcp4728));

	for (i = 0; i < priv->num; i++)
	{
		priv->mcp4728[i].sda = &priv->sda;
		priv->mcp4728[i].scl = &priv->scl;
		dev_read_u32_index(dev, "ldac", i,
				&priv->mcp4728[i].ldac);
		dev_read_u32_index(dev, "mcp4728,i2c-addr", i,
				&addr);
		priv->mcp4728[i].addr = addr;
	}

	return 0;
}

static const struct udevice_id drvids_mcp4728_i2cid[] = {
	{ .compatible = "mcp4728,i2cid" },
	{ }
};

U_BOOT_DRIVER(mcp4728_i2cid_drv) = {
	.name = "mcp4728_i2cid_drv",
	.id = UCLASS_MISC,
	.of_match = drvids_mcp4728_i2cid,
	.probe = drv_mcp4728_i2cid_probe,
	.of_to_plat = drv_mcp4728_i2cid_of_to_plat,
	.priv_auto = sizeof(struct mcp4728_i2cid_priv),
};
