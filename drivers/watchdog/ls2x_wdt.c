// SPDX-License-Identifier: GPL-2.0+
/*
 * loongson 2x soc built-in hardware watchdog timer
 *
 * Copyright (C) 2024 qujintao <qujintao@loongson.cn>
 *
 * Based on the Linux driver version which is:
 *   mt7621_wdt.c
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <wdt.h>
#include <asm/global_data.h>
#include <linux/io.h>

DECLARE_GLOBAL_DATA_PTR;

struct ls2x_wdt {
	void __iomem *regs;
	unsigned int clk_rate;
};

#define LS2X_WDT_RESET_REG		0x0
#define LS2X_WDT_SET_REG		0x4
#define LS2X_WDT_TIMER_REG		0x8

#define LS2X_WDT_ENABLE			(0x1 << 1)
#define LS2X_WDT_RESTART		0x1

static int ls2x_wdt_stop(struct udevice *dev);

static int ls2x_wdt_ping(struct ls2x_wdt *priv)
{
	writel(LS2X_WDT_RESTART, priv->regs + LS2X_WDT_SET_REG);

	return 0;
}

static int ls2x_wdt_start(struct udevice *dev, u64 ms, ulong flags)
{
	unsigned int temp;
	unsigned int value;
	u64 max_ms;
	u64 real_ms;
	struct ls2x_wdt *priv = dev_get_priv(dev);

	if (!ms)
		return 0;

	ls2x_wdt_stop(dev);

	max_ms = (unsigned int)(0xffffffff) / priv->clk_rate;
	max_ms *= 1000;

	real_ms = (max_ms <= ms) ? max_ms : ms;

	value = (real_ms) * (priv->clk_rate / 1000);
	/* set the prescaler to 1ms == 1000us */
	writel(value, priv->regs + LS2X_WDT_TIMER_REG);

	temp = readl(priv->regs + LS2X_WDT_RESET_REG);
	temp |= LS2X_WDT_ENABLE;
	writel(temp, priv->regs + LS2X_WDT_RESET_REG);

	ls2x_wdt_ping(priv);

	return 0;
}

static int ls2x_wdt_stop(struct udevice *dev)
{
	unsigned int temp;
	struct ls2x_wdt *priv = dev_get_priv(dev);

	ls2x_wdt_ping(priv);

	temp = readl(priv->regs + LS2X_WDT_RESET_REG);
	temp |= LS2X_WDT_ENABLE;
	temp ^= LS2X_WDT_ENABLE;
	writel(temp, priv->regs + LS2X_WDT_RESET_REG);

	return 0;
}

static int ls2x_wdt_reset(struct udevice *dev)
{
	struct ls2x_wdt *priv = dev_get_priv(dev);

	ls2x_wdt_ping(priv);

	return 0;
}

static int ls2x_wdt_probe(struct udevice *dev)
{
	struct clk clk;
	struct ls2x_wdt *priv = dev_get_priv(dev);
	int ret;

	priv->regs = dev_remap_addr(dev);
	if (!priv->regs)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return ret;

	ret = clk_enable(&clk);
	if (ret)
		return ret;

	priv->clk_rate = clk_get_rate(&clk);

	ls2x_wdt_stop(dev);

	return 0;
}

static const struct wdt_ops ls2x_wdt_ops = {
	.start = ls2x_wdt_start,
	.reset = ls2x_wdt_reset,
	.stop = ls2x_wdt_stop,
};

static const struct udevice_id ls2x_wdt_ids[] = {
	{ .compatible = "loongson,ls2x-wdt" },
	{}
};

U_BOOT_DRIVER(ls2x_wdt) = {
	.name = "ls2x_wdt",
	.id = UCLASS_WDT,
	.of_match = ls2x_wdt_ids,
	.probe = ls2x_wdt_probe,
	.priv_auto	= sizeof(struct ls2x_wdt),
	.ops = &ls2x_wdt_ops,
};
