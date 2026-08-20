// SPDX-License-Identifier: GPL-2.0+
/*
 * Loongson 2k USB PHY CONFIG driver
 *
 * Copyright (C) 2023 qujintao <qujintao@loongson.cn>
 * Copyright (C) 2023 LSGD
 *
 * Author: qujintao <qujintao@loongson.cn>
 */

#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <reset.h>
#include <linux/io.h>
#include <linux/bitops.h>

/*
 * 参考2k500的用户手册中关于 usb phy配置寄存器的位域说明
 * 本质上就是 对寄存器进行内容填充
 * 谨记 需要主动调用 phy_ls2k_usb_init(phy.h文件里面) 函数 才会自动加载这个驱动
 * e.g.
 * 在 dtsi 里面
	usb_config_phy0: usb_config_phy@1fe10500 {
			compatible = "ls2k-usb-phy";
			reg = <0 0x1fe10500 0 0x4>;
			status = "disable";
	};
 * 在 对应的  dts 里面
	&usb_config_phy0 {
		tx_res_tune = <0x1>;
		tx_vref_tune = <0x6>;
		tx_fsls_tune = <0x1>;
		status = "okay";
	};
 *
 */

#define PARAM_COMMON_ONNl_SHIFT 29
#define PARAM_COMMON_ONNl BIT(PARAM_COMMON_ONNl_SHIFT)

#define PARAM_DM_PULLDOWNl_SHIFT 28
#define PARAM_DM_PULLDOWNl BIT(PARAM_DM_PULLDOWNl_SHIFT)

#define PARAM_DP_PULLDOWNl_SHIFT 27
#define PARAM_DP_PULLDOWNl BIT(PARAM_DP_PULLDOWNl_SHIFT)

#define PARAM_TX_RES_TUNEl_SHIFT 25
#define PARAM_TX_RES_TUNEl GENMASK(26, PARAM_TX_RES_TUNEl_SHIFT)

#define PARAM_TX_PREEMP_PULSE_TUNEl_SHIFT 24
#define PARAM_TX_PREEMP_PULSE_TUNEl BIT(PARAM_TX_PREEMP_PULSE_TUNEl_SHIFT)

#define PARAM_TX_PREEMP_AMP_TUNEl_SHIFT 22
#define PARAM_TX_PREEMP_AMP_TUNEl GENMASK(23, PARAM_TX_PREEMP_AMP_TUNEl_SHIFT)

#define PARAM_TX_HSXV_TUNEl_SHIFT 20
#define PARAM_TX_HSXV_TUNEl GENMASK(21, PARAM_TX_HSXV_TUNEl_SHIFT)

#define PARAM_TX_RISE_TUNEl_SHIFT 18
#define PARAM_TX_RISE_TUNEl GENMASK(19, PARAM_TX_RISE_TUNEl_SHIFT)

#define PARAM_TX_VREF_TUNEl_SHIFT 14
#define PARAM_TX_VREF_TUNEl GENMASK(17, PARAM_TX_VREF_TUNEl_SHIFT)

#define PARAM_TX_FSLS_TUNEl_SHIFT 10
#define PARAM_TX_FSLS_TUNEl GENMASK(13, PARAM_TX_FSLS_TUNEl_SHIFT)

#define PARAM_SQ_RX_TUNEl_SHIFT 7
#define PARAM_SQ_RX_TUNEl GENMASK(9, PARAM_SQ_RX_TUNEl_SHIFT)

#define PARAM_COMPDIS_TUNEl_SHIFT 4
#define PARAM_COMPDIS_TUNEl GENMASK(6, PARAM_COMPDIS_TUNEl_SHIFT)

#define PARAM_OTG_TUNEl_SHIFT 1
#define PARAM_OTG_TUNEl GENMASK(3, PARAM_OTG_TUNEl_SHIFT)

struct prop_info {
	const char* prop_name;
	unsigned int shift;
	unsigned int mask;
};

struct prop_info prop_info_set[] = {
	{.prop_name = "common_onn", .shift = PARAM_COMMON_ONNl_SHIFT, .mask = PARAM_COMMON_ONNl},
	{.prop_name = "dm_pull_down", .shift = PARAM_DM_PULLDOWNl_SHIFT, .mask = PARAM_DM_PULLDOWNl},
	{.prop_name = "dp_pull_down", .shift = PARAM_DP_PULLDOWNl_SHIFT, .mask = PARAM_DP_PULLDOWNl},
	{.prop_name = "tx_res_tune", .shift = PARAM_TX_RES_TUNEl_SHIFT, .mask = PARAM_TX_RES_TUNEl},
	{.prop_name = "tx_preemp_pulse_tune", .shift = PARAM_TX_PREEMP_PULSE_TUNEl_SHIFT, .mask = PARAM_TX_PREEMP_PULSE_TUNEl},
	{.prop_name = "tx_preemp_amp_tune", .shift = PARAM_TX_PREEMP_AMP_TUNEl_SHIFT, .mask = PARAM_TX_PREEMP_AMP_TUNEl},
	{.prop_name = "tx_hsxv_tune", .shift = PARAM_TX_HSXV_TUNEl_SHIFT, .mask = PARAM_TX_HSXV_TUNEl},
	{.prop_name = "tx_rise_tune", .shift = PARAM_TX_RISE_TUNEl_SHIFT, .mask = PARAM_TX_RISE_TUNEl},
	{.prop_name = "tx_vref_tune", .shift = PARAM_TX_VREF_TUNEl_SHIFT, .mask = PARAM_TX_VREF_TUNEl},
	{.prop_name = "tx_fsls_tune", .shift = PARAM_TX_FSLS_TUNEl_SHIFT, .mask = PARAM_TX_FSLS_TUNEl},
	{.prop_name = "sq_rx_tune", .shift = PARAM_SQ_RX_TUNEl_SHIFT, .mask = PARAM_SQ_RX_TUNEl},
	{.prop_name = "compdis_tune", .shift = PARAM_COMPDIS_TUNEl_SHIFT, .mask = PARAM_COMPDIS_TUNEl},
	{.prop_name = "otg_tune", .shift = PARAM_OTG_TUNEl_SHIFT, .mask = PARAM_OTG_TUNEl},
	{.prop_name = NULL, .shift = 0, .mask = 0},
};

#define ENABLE_CFG_ENl BIT(0)

struct ls2k_usb_phy_priv {
	unsigned long long usb_phy_addr;
	unsigned int usb_phy_value;
	struct reset_ctl_bulk resets;
};

static int phy_ls2k_usb_power_on(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct ls2k_usb_phy_priv *priv = dev_get_priv(dev);
	uint val;

	val = priv->usb_phy_value;
	val |= ENABLE_CFG_ENl;
	writel(val, priv->usb_phy_addr);
	priv->usb_phy_value = val;

	return 0;
}

static int phy_ls2k_usb_power_off(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct ls2k_usb_phy_priv *priv = dev_get_priv(dev);
	uint val;

	val = priv->usb_phy_value;
	val &= ~(ENABLE_CFG_ENl);
	writel(val, priv->usb_phy_addr);
	priv->usb_phy_value = val;

	return 0;
}

static struct phy_ops ls2k_usb_phy_ops = {
	.power_on = phy_ls2k_usb_power_on,
	.power_off = phy_ls2k_usb_power_off,
};

static int read_param(struct udevice *dev, struct prop_info* info, unsigned int* value)
{
	int ret;
	unsigned int temp;
	if (unlikely(!value || !dev || !info))
		return -1;

	ret = dev_read_u32u(dev, info->prop_name, &temp);
	if (ret)
		return -2;

	temp <<= info->shift;
	temp &= info->mask;
	value[0] |= temp;
	return 0;
}

static int ls2k_usb_phy_probe(struct udevice *dev)
{
	struct ls2k_usb_phy_priv *priv = dev_get_priv(dev);
	unsigned int value = 0;
	struct prop_info* cur_prop_info;
	int i;

	priv->usb_phy_addr = (unsigned long long)dev_read_addr_ptr(dev);

	for (i = 0; ;++i) {
		cur_prop_info = prop_info_set + i;
		if (!cur_prop_info->prop_name)
			break;
		read_param(dev, cur_prop_info, &value);
	}

	if (value) {
		value |= ENABLE_CFG_ENl;
		writel(value, priv->usb_phy_addr);
	}

	return 0;
}

static int ls2k_usb_phy_remove(struct udevice *dev)
{
	struct ls2k_usb_phy_priv *priv = dev_get_priv(dev);

	return reset_release_bulk(&priv->resets);
}

static const struct udevice_id ls2k_usb_phy_ids[] = {
	{ .compatible = "ls2k-usb-phy" },
	{ }
};

U_BOOT_DRIVER(ls2k_usb_phy) = {
	.name = "ls2k_usb_phy",
	.id = UCLASS_PHY,
	.of_match = ls2k_usb_phy_ids,
	.probe = ls2k_usb_phy_probe,
	.remove = ls2k_usb_phy_remove,
	.ops = &ls2k_usb_phy_ops,
	.priv_auto = sizeof(struct ls2k_usb_phy_priv),
};

int phy_ls2k_usb_init(void)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_PHY, &uc);
	if (ret)
		return ret;
	for (uclass_first_device(UCLASS_PHY, &dev);
		dev;
		uclass_next_device(&dev)) {
	}

	return 0;
}

