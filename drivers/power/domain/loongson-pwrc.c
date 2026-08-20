// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2022 loongson(GD)
 * 
 *     powercontrol {
 *           enable-regs
 *           domain-dpm-regs
 *           clock-gate-regs
 *           domain0{
 *                   dpm-bit
 *                   on/off
 *                   device0-1{
 *                           clock-gate-bit
 *                           on/off
 *                   }
 *           }
 *           domain1{
 *                   dpm-bit
 *                   on/off
 *                   device1-0{
 *                           clock-gate-bit
 *                           on/off
 *                   }
 *           }
 *   }
 */
#include <common.h>
#include <dm.h>
#include <misc.h>
#include <asm/io.h>


struct ls_pwrc_priv {
	void __iomem *regs;
	u32 enable;
	// DPM
	u32 dpm;
	u32 dpm_offset;
	u32 dpm_bitwidth;
	// Clock Gate
	u32 cg;
	u32 cg_offset;
	u32 cg_bitwidth;
};

static int ls_pwrc_cg_off_in_domain(struct ls_pwrc_priv *priv, ofnode parent)
{
	u32 bit;
	ofnode node;

	ofnode_for_each_subnode(node, parent) {
		ofnode_read_u32(node, "bit", &bit);
		priv->cg |= 1 << bit;
	};

	return 0;
}

static int ls_pwrc_get_cg(struct ls_pwrc_priv *priv, ofnode parent)
{
	u32 bit;
	ofnode node;

	ofnode_for_each_subnode(node, parent) {
		ofnode_read_u32(node, "bit", &bit);
		if (ofnode_read_bool(node, "clock-gate-off"))
			priv->cg |= 1 << bit;
	};

	return 0;
}

static int ls_pwrc_get_dpm(struct ls_pwrc_priv *priv, ofnode parent)
{
	u32 bit;
	ofnode node;

	ofnode_for_each_subnode(node, parent) {
		ofnode_read_u32(node, "bit", &bit);
		if (ofnode_read_bool(node, "dpm-off"))
		{
			priv->dpm |= 3 << bit;
			ls_pwrc_cg_off_in_domain(priv, node);
		}
		else
		{
			ls_pwrc_get_cg(priv, node);
		}
	};

	return 0;
}

static int drv_ls_pwrc_of_to_plat(struct udevice *dev)
{
	struct ls_pwrc_priv *priv = dev_get_priv(dev);
	ofnode node = dev_ofnode(dev);
	
	priv->regs = dev_read_addr_ptr(dev);
	ofnode_read_u32(node, "enable", &priv->enable);
	ofnode_read_u32(node, "dpm", &priv->dpm_offset);
	ofnode_read_u32(node, "dpm-bitwidth", &priv->dpm_bitwidth);
	ofnode_read_u32(node, "clock-gate", &priv->cg_offset);
	ofnode_read_u32(node, "clock-gate-bitwidth", &priv->cg_bitwidth);

	return 0;
}

static int drv_ls_pwrc_probe(struct udevice *dev)
{
	struct ls_pwrc_priv *priv = dev_get_priv(dev);
	ofnode node = dev_ofnode(dev);

	writel(priv->enable, priv->regs);

	ls_pwrc_get_dpm(priv, node);

	writel(priv->dpm, priv->regs + priv->dpm_offset);
	writel(priv->cg, priv->regs + priv->cg_offset);

	return 0;
}

static const struct udevice_id drvid_ls_pwrc[] = {
	{ .compatible = "loongson,pwrc" },
	{ }
};

U_BOOT_DRIVER(loongson_pwrc) = {
	.name = "loongson-pwrc",
	.id = UCLASS_MISC,
	.of_match = drvid_ls_pwrc,
	.of_to_plat = drv_ls_pwrc_of_to_plat,
	.probe = drv_ls_pwrc_probe,
	.priv_auto = sizeof(struct ls_pwrc_priv),
};

