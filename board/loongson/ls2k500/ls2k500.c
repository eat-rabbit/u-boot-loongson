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

	/* config all i2c as master */
	writeb(readb(LS_I2C0_REG_BASE + 2) | 0x20, LS_I2C0_REG_BASE + 2);
	writeb(readb(LS_I2C0_REG_BASE + 0x802) | 0x20, LS_I2C0_REG_BASE + 0x802);
	writeb(readb(LS_I2C0_REG_BASE + 0x1002) | 0x20, LS_I2C0_REG_BASE + 0x1002);
	writeb(readb(LS_I2C0_REG_BASE + 0x1802) | 0x20, LS_I2C0_REG_BASE + 0x1802);
	writeb(readb(LS_I2C0_REG_BASE + 0x2002) | 0x20, LS_I2C0_REG_BASE + 0x2002);
	writeb(readb(LS_I2C0_REG_BASE + 0x2802) | 0x20, LS_I2C0_REG_BASE + 0x2802);

	val = readl(LS_GENERAL_CFG0);
	val |= (1 << 9);	//hda pins use ac97
	writel(val, LS_GENERAL_CFG0);

	val = readl(LS_GENERAL_CFG1);
	val |= (1 << 1) | (1 << 28);	// usb0 as otg, lio 16bit
	writel(val, LS_GENERAL_CFG1);

	/*usb must init by the flow code otherwise cause error*/
	val = readl(LS_GENERAL_CFG1);
	val = (val & ~0x3fd) | 0x3e5;
	writel(val, LS_GENERAL_CFG1);

	/* SATA default setting */
	val = readl(LS_SATA0_REG0_CFG);
	val = (val & ~(0xff << 16)) | (0x9f << 16);
	writel(val, LS_SATA0_REG0_CFG);

	/* uart 0 and 1 be 2 line mode */
	val = readl(LS_GENERAL_CFG0);
	val |= 0xee;
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

#ifdef CONFIG_SCSI_AHCI
static void ahci_setup(void)
{

}
#endif

#ifdef CONFIG_SCSI_AHCI_PLAT
void scsi_init(void)
{
	void __iomem *ahci_base;

	ahci_base = (void __iomem *)0x1f040000;
	printf("scsi ahci plat %p\n", ahci_base);
	if(!ahci_init(ahci_base))
		scsi_scan(1);
}
#endif

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

#ifdef CONFIG_USB_XHCI_DWC3
static void xhci_setup(void)
{
	void __iomem *phy_base = (unsigned int *)LS_XHCI_BASE;

	u32 val = 0;
	u32 cfg = 0xff04;
	val = readl(phy_base + cfg);
	val |= (0x1 << 18);
	writel(val, phy_base + cfg);

	cfg = 0xc110;
	val = readl(phy_base + cfg);
	val |= (0x1 << 12);
	val &= (~(0x1 << 13));
	writel(val, phy_base + cfg);

	cfg = 0xff00;
	val = readl(phy_base + cfg);
	val &= (~(0x1 << 2));
	writel(val, phy_base + cfg);

	cfg = 0xc2c0;
	val = readl(phy_base + cfg);
	val &= (~(0x3 << 1));
	writel(val, phy_base + cfg);
}
#endif

static void ls2x_pcie_phy_write(u32 cfg_val)
{
	void __iomem *reg;

	u32 val = 0;
	reg = ioremap(0x1fe10560, 1);
	writel(cfg_val, reg);
	writel(0x1, reg + 0x4);
	do {
		val = readl(reg + 0x4);
	} while ((val & 0x4) == 0);
}

static void ls2x_pcie_port_conf(int port)
{
	u32 val = 0;
	void __iomem *reg;

	reg = ioremap(0xfe0800000c | (port << 11), 1);
	val = readl(reg);
	val = val & 0xfff9ffff;
	val = val | 0x00020000;
	writel(val, reg);

	reg = ioremap(0xfe0700001c | (port << 11), 1);
	val = readl(reg);
	val = val | (0x1 << 26);
	writel(val, reg);

	reg = ioremap(0x16000000 | (port << 1), 1);
	val = readl(reg + 0x54);
	val = val & (~((0x7 << 18) | (0x7 << 2)));
	writel(val, reg + 0x54);
	val = readl(reg + 0x58);
	val = val & (~((0x7 << 18) | (0x7 << 2)));
	writel(val, reg + 0x58);

	writel(0x00ff204f, reg);
}

static void pcie_setup(void)
{
	u32 val = 0;
	void __iomem *reg;

	ls2x_pcie_phy_write(0x4fff1002);
	ls2x_pcie_phy_write(0x4fff1102);
	ls2x_pcie_phy_write(0x4fff1202);
	ls2x_pcie_phy_write(0x4fff1302);

	ls2x_pcie_port_conf(0);
	ls2x_pcie_port_conf(1);

	reg = ioremap(0x1fe10120, 1);
	val = readl(reg);
	if ((val & (0x1<6)) && (val & (0x1<11))) {
		/*set ep device 0,1 class type to 0x0600*/
		void __iomem *port_ctrl = ioremap(0x16000000, 1);
		void __iomem *port_cfg = ioremap(0x16800000, 1);
		writel(0x21ff204f, port_ctrl);
		writel(0x06000001, port_cfg + 0x8);
		writel(0x06000001, port_cfg + 0x1000 + 0x8);
		writel(0x01ff204f, port_ctrl);
		/*set ep device 0,1 bar size to 256M*/
		writel(0x0fffffff, port_cfg + 0x110);
		writel(0x0fffffff, port_cfg + 0x1000 + 0x110);
		writel(0x000f0000, port_ctrl + 0x68);
		writel(0x00000000, port_ctrl + 0x70);
	} else {
		/*set pcie controler device 0 bar size to 4K*/
//		void __iomem *port_cfg = ioremap(0xfe00000000, 1);
		void __iomem *port_cfg = ioremap(0x16800000, 1);
		writel(0x00000fff, port_cfg + 0x110);
		writel(0x00000fff, port_cfg + 0x800 + 0x110);
	}
}

static void regulator_init(void)
{
#ifdef CONFIG_DM_REGULATOR
	regulators_enable_boot_on(false);
#endif
}

#ifdef CONFIG_BOARD_EARLY_INIT_R
int ls_board_early_init_r(void)
{
#ifdef CONFIG_VIDEO_BRIDGE_MS1824
	uclass_probe_all(UCLASS_VIDEO_BRIDGE);
#endif
#ifdef CONFIG_USB_XHCI_DWC3
	/*init dwc3*/
	xhci_setup();
#endif

	pcie_setup();

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

static void dpm_init(void)
{
	uclass_probe_all(UCLASS_MISC);
}

#ifdef CONFIG_BOARD_LATE_INIT
int ls_board_late_init(void)
{
	if(IS_ENABLED(CONFIG_LS2K500_POWER_DOMAIN)) {
		dpm_init();
	}

	if (IS_ENABLED(CONFIG_LED)) {
		led_default_state();
	}

	return 0;
}
#endif

