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

static void dev_fixup(void)
{
	u32 val;

	// emmc pinmux
	val = readl(PHYS_TO_UNCACHED(0x14000490));
	val &= ~0x0000ff00;
	val |= 0x0000aa00;
	writel(val, PHYS_TO_UNCACHED(0x14000490));
	val = readl(PHYS_TO_UNCACHED(0x14000494));
	writel(val | 0x00000fff, PHYS_TO_UNCACHED(0x14000494));

	// /* config all i2c as master */
	// writeb(readb(LS_I2C0_REG_BASE + 2) | 0x20, LS_I2C0_REG_BASE + 2);
	// writeb(readb(LS_I2C0_REG_BASE + 0x802) | 0x20, LS_I2C0_REG_BASE + 0x802);
	// writeb(readb(LS_I2C0_REG_BASE + 0x1002) | 0x20, LS_I2C0_REG_BASE + 0x1002);
	// writeb(readb(LS_I2C0_REG_BASE + 0x1802) | 0x20, LS_I2C0_REG_BASE + 0x1802);
	// writeb(readb(LS_I2C0_REG_BASE + 0x2002) | 0x20, LS_I2C0_REG_BASE + 0x2002);
	// writeb(readb(LS_I2C0_REG_BASE + 0x2802) | 0x20, LS_I2C0_REG_BASE + 0x2802);

	// val = readl(LS_GENERAL_CFG0);
	// val |= (1 << 9);	//hda pins use ac97
	// writel(val, LS_GENERAL_CFG0);

	// val = readl(LS_GENERAL_CFG1);
	// val |= (1 << 1) | (1 << 28);	// usb0 as otg, lio 16bit
	// writel(val, LS_GENERAL_CFG1);

	// /*usb must init by the flow code otherwise cause error*/
	// val = readl(LS_GENERAL_CFG1);
	// val = (val & ~0x3fd) | 0x3e5;
	// writel(val, LS_GENERAL_CFG1);


	/* uart 0 and 1 be 2 line mode */
	val = readl(LS_GENERAL_CFG0);
	val |= 0xe0008000;
	writel(val, LS_GENERAL_CFG0);

	/*RTC toytrim rtctrim must init to 0, otherwise time can not update*/
	writel(0x0, LS_TOY_TRIM_REG);
	writel(0x0, LS_RTC_TRIM_REG);
}

/*enable fpu regs*/
static void __maybe_unused fpu_enable(void)
{
	asm (
		"csrrd $r4, 0x2;\n\t"
		"ori   $r4, $r4, 1;\n\t"
		"csrwr $r4, 0x2;\n\t"
		:::"$r4"
	);
}

/*disable fpu regs*/
static void __maybe_unused fpu_disable(void)
{
	asm (
		"csrrd $r4, 0x2;\n\t"
		"ori   $r4, $r4, 1;\n\t"
		"xori  $r4, $r4, 1;\n\t"
		"csrwr $r4, 0x2;\n\t"
		:::"$r4"
	);
}


#ifdef CONFIG_BOARD_EARLY_INIT_F
int ls_board_early_init_f(void)
{
	// fpu_enable();
	dev_fixup();
#ifdef CONFIG_SCSI_AHCI
	ahci_setup();
#endif
	return 0;
}
#endif

static void regulator_init(void)
{
#ifdef CONFIG_DM_REGULATOR
	regulators_enable_boot_on(false);
#endif
}

#ifdef CONFIG_BOARD_EARLY_INIT_R
int ls_board_early_init_r(void)
{
	regulator_init();

	return 0;
}
#endif

static void dpm_init(void)
{
	uclass_probe_all(UCLASS_MISC);
}

#ifdef CONFIG_BOARD_LATE_INIT
int ls_board_late_init(void)
{
#if defined(CONFIG_LS2K500_POWER_DOMAIN) || defined(CONFIG_LOONGSON_POWER_DOMAIN)
	dpm_init();
#endif
	if (IS_ENABLED(CONFIG_LED)) {
		led_default_state();
	}

	return 0;
}
#endif

