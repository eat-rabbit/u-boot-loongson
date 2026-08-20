// SPDX-License-Identifier: GPL-2.0
/*
 * Loongson PCI Express Root-Complex driver
 *
 * Copyright (C) 2019 TangHaifeng <tang-haifeng@foxmail.com>
 *
 */

#include <common.h>
#include <pci.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <errno.h>

#include <mach/loongson.h>

void ls2x_port_setup(int port, u32 pcie_port_bar_addr)
{
	u32 val = 0;
	u32 val_t = 0;
	u32 port_l = port << 11;

	// 配置为 Gen2
	val = readl(PHYS_TO_UNCACHED(0xfe0800000c) | port_l);
	val &= 0xfffbffff;
	val |= 0x20000;
	writel(val, PHYS_TO_UNCACHED(0xfe0800000c) | port_l);

	//来自 pmon 的初始化代码
	val = readl(PHYS_TO_UNCACHED(0xfe0700001c) | port_l);
	val |= 1 << 26;
	writel(val, PHYS_TO_UNCACHED(0xfe0700001c) | port_l);

	//来自 pmon 的初始化代码
	val = readl((PHYS_TO_UNCACHED(0xfe00000000) | port_l) + 0x78);
	val |= 0x7 << 12;
	val ^= 0x7 << 12;
	val |= 0x1000;
	writel(val, (PHYS_TO_UNCACHED(0xfe00000000) | port_l) + 0x78);

	// 配置 对应 port 的bar地址
	writel(pcie_port_bar_addr, (PHYS_TO_UNCACHED(0xfe00000000) | port_l) + 0x10);

	//来自 pmon 的初始化代码
	val = readl(PHYS_TO_UNCACHED(pcie_port_bar_addr) + 0x54);
	val_t = ~((0x7 << 18) | (0x7 << 2));
	val &= val_t;
	writel(val, (PHYS_TO_UNCACHED(pcie_port_bar_addr) + 0x54));

	//来自 pmon 的初始化代码
	val = readl(PHYS_TO_UNCACHED(pcie_port_bar_addr) + 0x58);
	val &= val_t;
	writel(val, (PHYS_TO_UNCACHED(pcie_port_bar_addr) + 0x58));

	// 链路训练的动作启动 正常来说要等到训练完成 但是这里统一在最后 mdelay(50);
	writel(0xff200c, (PHYS_TO_UNCACHED(pcie_port_bar_addr)));
}

void ls2x_pcie0_setup(void)
{
	u32 val = 0;

	//pcie phy 配置
	writel(0xc2492331, LS_PCIE0_CONF0);
	writel(0xff3ff0a8, LS_PCIE0_CONF0 + 4);
	writel(0x00027fff, LS_PCIE0_CONF1);

	writel(0x4fff1002, LS_PCIE0_PHY);
	writel(0x1, LS_PCIE0_PHY + 4);
	mdelay(5);

	writel(0x4fff1102, LS_PCIE0_PHY);
	writel(0x1, LS_PCIE0_PHY + 4);
	mdelay(5);

	writel(0x4fff1202, LS_PCIE0_PHY);
	writel(0x1, LS_PCIE0_PHY + 4);
	mdelay(5);

	writel(0x4fff1302, LS_PCIE0_PHY);
	writel(0x1, LS_PCIE0_PHY + 4);
	mdelay(5);

	//使能pcie
	val = readl(LS_GENERAL_CFG2);
	val = val | LS_CFG2_PCIE0_ENABLE;
	writel(val, LS_GENERAL_CFG2);
	mdelay(5);

	//配置头设置
	//pcie0 port0
	ls2x_port_setup(9, 0x11000000);
	// writel(0x11000000, (LS_PCIE_TYPE0_ADDR | 0x4800 | 0x10));
	// writel(0x00ff204c, PHYS_TO_UNCACHED(0x11000000));  //这里保留是 0x00ff204c 因为不知道意思是啥
	//pcie0 port1
	ls2x_port_setup(10, 0x11100000);
	//pcie0 port2
	ls2x_port_setup(11, 0x11200000);
	//pcie0 port3
	ls2x_port_setup(12, 0x11300000);
}

void ls2x_pcie1_setup(void)
{
	u32 val = 0;

	//pcie phy 配置
	writel(0xc2492331, LS_PCIE1_CONF0);
	writel(0xff3ff0a8, LS_PCIE1_CONF0 + 4);
	writel(0x00027fff, LS_PCIE1_CONF1);


	writel(0x4fff1002, LS_PCIE1_PHY);
	writel(0x1, LS_PCIE1_PHY + 4);
	mdelay(5);

	writel(0x4fff1102, LS_PCIE1_PHY);
	writel(0x1, LS_PCIE1_PHY + 4);
	mdelay(5);

	writel(0x4fff1202, LS_PCIE1_PHY);
	writel(0x1, LS_PCIE1_PHY + 4);
	mdelay(5);

	writel(0x4fff1302, LS_PCIE1_PHY);
	writel(0x1, LS_PCIE1_PHY + 4);
	mdelay(5);

	//使能pcie
	val = readl(LS_GENERAL_CFG2);
	val = val | LS_CFG2_PCIE1_ENABLE;
	writel(val, LS_GENERAL_CFG2);
	mdelay(5);

	//pcie1 port0
	ls2x_port_setup(13, 0x10000000);

	//pcie1 port1
	ls2x_port_setup(14, 0x10100000);
}

void ls2x_pcie_msi_window_config(void)
{
	writeq(0x000000001fe10000ULL, PHYS_TO_UNCACHED(0x1fe02500));
	writeq(0xffffffffffff0000ULL, PHYS_TO_UNCACHED(0x1fe02540));
	writeq(0x000000001fe10081ULL, PHYS_TO_UNCACHED(0x1fe02580));
}

void ls2x_pcie_setup(void)
{
	ls2x_pcie0_setup();
	ls2x_pcie1_setup();
	ls2x_pcie_msi_window_config();
	mdelay(50);//等待pcie reset信号完成
}
