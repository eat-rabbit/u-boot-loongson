// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <env.h>
#include <pci.h>
#include <usb.h>
#include <scsi.h>
#include <ahci.h>
#include <led.h>
#include <asm/io.h>
#include <dm.h>
#include <mach/loongson.h>
#include <power/regulator.h>
#include "../common/bdinfo/bdinfo.h"

extern void update_slave_core(void);
extern void ls2x_pcie_setup(void);

static void mem_win_cfg(void)
{
	/* CPU_WIN0 */
	writeq(0x1c000000, LS_CPU_WIN0_BASE);
	writeq(0xfffffffffff00000, LS_CPU_WIN0_MASK);
	// writeq(0x1fc000f2, LS_CPU_WIN0_MMAP);
	// disable instruct fetch for spi io mode
	writeq(0x1fc00082, LS_CPU_WIN0_MMAP);

	/* CPU_WIN1 */
	writeq(0x10000000, LS_CPU_WIN1_BASE);
	writeq(0xfffffffff0000000, LS_CPU_WIN1_MASK);
	writeq(0x10000082, LS_CPU_WIN1_MMAP);

	/* CPU_WIN2 */
	writeq(0x0, LS_CPU_WIN2_BASE);
	writeq(0xfffffffff0000000, LS_CPU_WIN2_MASK);
	writeq(0xf0, LS_CPU_WIN2_MMAP);

	/* CPU_WIN3 */
	writeq(0x80000000, LS_CPU_WIN3_BASE);
	writeq(0xffffffff80000000, LS_CPU_WIN3_MASK);
	writeq(0xf0, LS_CPU_WIN3_MMAP);

	/* CPU_WIN4 */
	writeq(0x100000000, LS_CPU_WIN4_BASE);
	writeq(0xffffffff00000000, LS_CPU_WIN4_MASK);
	writeq(0x1000000f0, LS_CPU_WIN4_MMAP);

	/* CPU_WIN5 */
	writeq(0x200000000, LS_CPU_WIN3_BASE);
	writeq(0xfffffffe00000000, LS_CPU_WIN3_MASK);
	writeq(0x2000000f0, LS_CPU_WIN5_MMAP);

	/* CPU_WIN6 */
	writeq(0x0, LS_CPU_WIN6_BASE);
	writeq(0x0, LS_CPU_WIN6_MASK);
	writeq(0x0, LS_CPU_WIN6_MMAP);

	/* CPU_WIN7 */
	writeq(0x0, LS_CPU_WIN7_BASE);
	writeq(0x0, LS_CPU_WIN7_MASK);
	writeq(0x0, LS_CPU_WIN7_MMAP);
}

static void dev_fixup(void)
{
	/* Disable USB prefetch */
	writel(readl(LS_GENERAL_CFG1) & ~(1 << 19), LS_GENERAL_CFG1);

	/*RTC toytrim rtctrim must init to 0, otherwise time can not update*/
	writel(0x00, LS_RTC_REG_BASE + 0x20);
	writel(0x00, LS_RTC_REG_BASE + 0x60);
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int ls_board_early_init_f(void)
{
	// sync core1 to run in ram.
	update_slave_core();

	mem_win_cfg();
	dev_fixup();

	return 0;
}
#endif

static void regulator_init(void)
{
#ifdef CONFIG_DM_REGULATOR
	regulators_enable_boot_on(false);
#endif
}

static void low_power_setup(void)
{
	u32 val = 0;
	u32 tmp = 0;

	u32 cam_disable = 0;
	u32 vpu_disable = 0;
	u32 pcie0_enable = 1;
	u32 pcie1_enable = 1;

	int node;

	node = fdt_path_offset(gd->fdt_blob, "/soc");
	if (node >= 0) {
		fdtdec_get_int_array(gd->fdt_blob, node, "cam_disable", &cam_disable, 1);
		fdtdec_get_int_array(gd->fdt_blob, node, "vpu_disable", &vpu_disable, 1);
		fdtdec_get_int_array(gd->fdt_blob, node, "pcie0_enable", &pcie0_enable, 1);
		fdtdec_get_int_array(gd->fdt_blob, node, "pcie1_enable", &pcie1_enable, 1);
	}
	printf("cam_disable:%d, vpu_disable:%d, pcie0_enable:%d, pcie1_enable:%d\n",
				cam_disable, vpu_disable, pcie0_enable, pcie1_enable);

	val = readl(LS_GENERAL_CFG2);
	if (cam_disable)
		val |= LS_CFG2_CAM_DISABLE;

	if (vpu_disable)
		val |= LS_CFG2_VPU_DISABLE;

	if (!pcie0_enable) {
		tmp = readl(LS_PCIE0_CONF1);
		tmp |= LS_PCIE_LOW_POWER;
		writel(tmp, LS_PCIE0_CONF1);

		val &= ~(LS_CFG2_PCIE0_ENABLE);
	}

	if (!pcie1_enable) {
		tmp = readl(LS_PCIE1_CONF1);
		tmp |= LS_PCIE_LOW_POWER;
		writel(tmp, LS_PCIE1_CONF1);

		val &= ~(LS_CFG2_PCIE1_ENABLE);
	}

	writel(val, LS_GENERAL_CFG2);
}

static void ls2x_gpio_mux_init(void)
{
	u64 gen_config_0 = PHYS_TO_UNCACHED(0x1fe00420);
	u32 val = readl(gen_config_0);

	// hda i2s disable
	val |= (1 << 4) | (1 << 6);
	val ^= (1 << 4) | (1 << 6);
	writel(val, gen_config_0);
}

#ifdef CONFIG_BOARD_EARLY_INIT_R
int ls_board_early_init_r(void)
{
	ls2x_pcie_setup();
	low_power_setup();
	ls2x_gpio_mux_init();
#ifdef CONFIG_DM_PCI
	/*
	 * Make sure PCI bus is enumerated so that peripherals on the PCI bus
	 * can be discovered by their drivers
	 */
	pci_init();
#endif

	regulator_init();

	return 0;
}
#endif

#ifdef CONFIG_TARGET_LS2K1000_DP
void enable_fan(void)
{
	u8 gpio_fan = 57;
	u64 gpio_oen = PHYS_TO_UNCACHED(0x1fe00500);
	u64 val = readq(gpio_oen);
	u64 tmp = ~(((u64)0x1) << gpio_fan);
	val =val & tmp;
	writeq(val, gpio_oen);

	u64 gpio_o = PHYS_TO_UNCACHED(0x1fe00510);
	val = readq(gpio_o);
	val |= (((u64)0x1) << gpio_fan);
	writeq(val, gpio_o);
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int ls_board_late_init(void)
{
	if (IS_ENABLED(CONFIG_LED)) {
		led_default_state();
	}
#ifdef CONFIG_TARGET_LS2K1000_DP
	enable_fan();
#endif

	return 0;
}
#endif

#ifdef CONFIG_SPL_BUILD
#ifdef CONFIG_SPL_BOARD_INIT
void spl_board_init(void)
{
}
#endif
#endif
