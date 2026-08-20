#include <config.h>
#include <linux/linkage.h>
#include <asm/regdef.h>
#include <asm/addrspace.h>
#include <asm/loongarch.h>
#include <mach/loongson.h>

#ifndef CONFIG_SYS_INIT_SP_ADDR
#define CONFIG_SYS_INIT_SP_ADDR		PHYS_TO_CACHED(0x02000000)	// 32MB
#endif

// Note: the Address mapping window NOT setup yet.
// 		 so, use the real physicl address.
.macro watchdog_open
	//enable watch DOG.
	li.d	t1, LS_ACPI_REG_BASE
	li.w	t2, 0x2fffffff
	st.w	t2, t1, 0x38

	ld.w	t2, t1, 0x30
	li.w	t3, 0x2
	or	    t2, t2, t3
	st.w	t2, t1, 0x30	//enable watchdog

	li.w	t2, 0x1
	st.w	t2, t1, 0x34	//set watchdog time
.endm

.macro watchdog_close
	//disable watch DOG.
	li.d	t1, LS_ACPI_REG_BASE
	ld.w	t2, t1, 0x30
	li.w	t3, ~0x2
	and	    t2, t2, t3
	st.w	t2, t1, 0x30
.endm

ENTRY(lowlevel_init)
    or     a7, ra, zero

	/* slave core run to slave_main */
	csrrd   t0, LOONGARCH_CSR_CPUNUM
	andi    t0, t0, CSR_CPUNUM_CID	 /* cpu id */
	li.d    a0, CACHED_MEMORY_ADDR
	andi    t1, t0, 0x3       		 /* core id */
	slli.d  t2, t1, 18
	or      a0, t2, a0              /* 256KB offset for the each core */
	andi    t2, t0, 0xc             /* node id */
	slli.d  t2, t2, 42
	or      a0, t2, a0              /* get the L2 cache address */

	slli.d  t1, t1, 8
	or      t1, t2, t1

	li.d    t2, LS_CORE0_IPISR
	or      t1, t2, t1

	li.d    t3, RESERVED_COREMASK
	andi    t3, t3, 0xf
	li.d    t1, 0x1
	sll.w   t1, t1, t0
	and     t3, t1, t3
	bnez    t3, wait_to_be_killed

	li.d    t2, BOOTCORE_ID
	bne     t0, t2, slave_main

	li.d    a0, LS_FREQ_SCALE
	ld.w    t2, a0, 4
	xori    t2, t2, SHUTDOWN_MASK
	st.w    t2, a0, 4

	b       1f

wait_to_be_killed:
	b	wait_to_be_killed
	nop

1:
    /* config pci bar for APB, set base addr */
	li.d	t0, LS_PCIE_APB_ADDR
	li.d	t1, LS_APB_BASE
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for GMAC0, and set base addr */
	li.d	t0, LS_PCIE_GMAC0_ADDR
	li.d	t1, 0x40040000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for GMAC1, set base addr */
    li.d	t0, LS_PCIE_GMAC1_ADDR
	li.d	t1, 0x40050000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for otg, set base addr */
    li.d	t0, LS_PCIE_USB_OTG_ADDR
	li.d	t1, 0x40000000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for ehci, set base addr */
    li.d	t0, LS_PCIE_USB_EHCI_ADDR
	li.d	t1, 0x40060000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for ohci, set base addr */
	li.d	t0, LS_PCIE_USB_OHCI_ADDR
	li.d	t1, 0x40070000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for DC, set base addr */
	li.d	t0, LS_PCIE_DC_ADDR
	li.d	t1, 0x400c0000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for HDA, set base addr */
	li.d	t0, LS_PCIE_HDA_ADDR
	li.d	t1, 0x400d0000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	/* config pci bar for SATA, set base addr */
    li.d	t0, LS_PCIE_SATA_ADDR
	li.d	t1, 0x400e0000
	st.w	t1,	t0,	LS_PCIE_TYPE0_BAR0_OFFSET
	ld.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET
	ori	    t2,	t2,	LS_PCIE_CMD_MEM_ENABLE|7
	st.w	t2,	t0,	LS_PCIE_TYPE0_CMD_OFFSET

	watchdog_close

	/* ACPI Power Button Status clear */
	li.d	t0, LS_ACPI_REG_BASE
	ld.w	t1, t0, LS_ACPI_PM1_STS_OFFSET
	ori		t1, t1, LS_PM1_STS_PWRBTN_STS
	st.w	t1, t0, LS_ACPI_PM1_STS_OFFSET

/*
* we will init pcie in c env
* file:board/loongson/2k1000/pcie.c
*/
#if 0
	//pcie signal test copy
	li.w	t1, 0xc2492331
	li.d	t0, LS_PCIE0_CONF0
	st.w	t1, t0, 0
	li.d	t0, LS_PCIE1_CONF0
	st.w	t1, t0, 0

	li.w	t1, 0xff3ff0a8
	li.d	t0, LS_PCIE0_CONF0
	st.w	t1, t0, 0x4
	li.d	t0, LS_PCIE1_CONF0
	st.w	t1, t0, 0x4

	li.w	t1, 0x27fff
	li.d	t0, LS_PCIE0_CONF1
	st.w	t1, t0, 0
	li.d	t0, LS_PCIE1_CONF1
	st.w	t1, t0, 0


/* cfg pcie copy*/
	li.d	a0, 0x4fff1002
	bl	ls2k_pcie_phy_write
	li.d	a0, 0x4fff1102
	bl	ls2k_pcie_phy_write
	li.d	a0, 0x4fff1202
	bl	ls2k_pcie_phy_write
	li.d	a0, 0x4fff1302
	bl	ls2k_pcie_phy_write

	li.d	t0, LS_GENERAL_CFG2
	ld.w	t1, t0, 0
	li.w	t2, (LS_CFG2_PCIE0_ENABLE | LS_CFG2_PCIE1_ENABLE)
	or		t1, t1, t2	//pcie enable
	st.w	t1, t0, 0

/* pcie 0 port 0 */
	li.d	a0, 9
	bl	ls2k_pcie0_port_conf
/* pcie 0 port 1 */
	li.d	a0, 10
	bl	ls2k_pcie0_port_conf
/* pcie 0 port 2 */
	li.d	a0, 11
	bl	ls2k_pcie0_port_conf
/* pcie 0 port 3 */
	li.d	a0, 12
	bl	ls2k_pcie0_port_conf
/* pcie 1 port 0 */
	li.d	a0, 13
	bl	ls2k_pcie1_port_conf
/* pcie 1 port 1 */
	li.d	a0, 14
	bl	ls2k_pcie1_port_conf
#endif

#include "clksetting.S"

	/* Config SATA : use internel clock */
	li.d	t0, LS_SATA_CONF
    li.w    t1, 0x30c31cf9
    st.w    t1, t0, 0x4
    li.w    t1, 0xf300040d
    st.w    t1, t0, 0

	/* reset sata */
	ld.w	t1, t0, 0
	ori		t1, t1, 0x4
	st.w	t1, t0, 0

	ld.w	t1, t0, 0
	ori		t1, t1, 0x8
	st.w	t1, t0, 0

	li.w    t1, 0x30c31cf9
    st.w    t1, t0, 0x4

	/* Config SATA TX signal*/
	li.d	t1, 0x1403f1002
	st.d   	t1, t0, 0x8

 	// Fix the Gmac0  multi-func to enable Gmac1
	li.d	t0, LS_PCICFG2_RECFG
	li.d	a0, 0xffffff0000ffffff
	st.d	a0, t0, 0x08

	li.d	t0, 0xfe00001800
	li.w	a0, 0x0080ff08
	st.w	a0, t0, 0x0c

	// Set the invalid BAR to read only
	li.d	t0, LS_PCICFG2_RECFG
	li.d	a0, 0xff00ff0000fffff0
	st.d	a0, t0, 0x00
	st.d	a0, t0, LS_PCICFG2_GMAC0_OFFSET
	st.d	a0, t0, LS_PCICFG2_GMAC1_OFFSET
	st.d	a0, t0, LS_PCICFG2_USB_OTG_OFFSET
	st.d	a0, t0, LS_PCICFG2_USB_EHCI_OFFSET
	st.d	a0, t0, LS_PCICFG2_USB_OHCI_OFFSET
	st.d	a0, t0, LS_PCICFG2_GPU_OFFSET
	st.d	a0, t0, LS_PCICFG2_DC_OFFSET
	st.d	a0, t0, LS_PCICFG2_HDA_OFFSET
	st.d	a0, t0, LS_PCICFG2_SATA_OFFSET
	st.d	a0, t0, LS_PCICFG2_DMA_OFFSET

	li.d	t0, LS_GENERAL_CFG2
	ld.w 	a2, t0, 0
	// enable pcie0 and pcie1, dvo0 and dvo1 pin output
	li.w	t1, (LS_CFG2_DVO0_SEL | LS_CFG2_DVO1_SEL)
	or	a2, a2, t1
   	st.w    a2, t0, 0


	li.d	t0, LS_GENERAL_CFG0
	ld.w	a2, t0, 0
	//enable sdio,pwm0, pwm1, i2c0, i2c1, nand, sata, i2s, gmac1
	//no hda
	li.w	t1, (LS_CFG0_SDIO_SEL | LS_CFG0_PWM1_SEL | LS_CFG0_PWM0_SEL | \
				LS_CFG0_I2C1_SEL | LS_CFG0_I2C0_SEL | LS_CFG0_NAND_SEL | \
				LS_CFG0_SATA_SEL | LS_CFG0_I2S_SEL | LS_CFG0_GMAC1_SEL)
	or	a2, a2, t1
	st.w	a2, t0, 0

	/* spi speedup */
	li.d	t0, LS_SPI_IO_REG_BASE
	li.w	t1, 0x47
	st.b	t1, t0, 0x4


    or      ra, a7, zero
    jirl	zero, ra, 0
ENDPROC(lowlevel_init)


/*
 * a0 [0:15] phy configure address, [16:31] phy configure data
 * [32]  phy_cfg_rw, 1 for write, 0 for read
 */
ENTRY(ls2k_pcie_phy_write)
	li.d	a1, 0x100000000		//enable write
	or		a0, a1, a0
	li.d	a1, LS_PCIE0_PHY
	st.d	a0, a1, 0x0
	st.d	a0, a1, LS_PCIE0_1_OFFSET
1:
	ld.w	a0, a1, 0x4
	andi	a0, a0, (1 << 2)	// [32+2]check phy_cfg_done or not
	beqz	a0, 1b
	jirl	zero, ra, 0
ENDPROC(ls2k_pcie_phy_write)

/*
 * a0 is device number
 */
ENTRY(ls2k_pcie0_port_conf)
	slli.w	a0, a0, LS_PCIE_CONF_HDR_DEV_NUM_OFFSET
	li.d	a1, 0xfe0800000c
	or		a1, a1, a0

	li.w	a2, 0xfff9ffff
	ld.w	a3, a1, 0x0
	and		a3, a3, a2
	li.w	a2, 0x20000
	or		a3, a3, a2
	st.w	a3, a1, 0x0

	li.d	a1, 0xfe0700001c
	or		a1, a1, a0
	ld.w	a3, a1, 0x0
	li.w	a2, (0x1 << 26)
	or		a3, a3, a2
	st.w	a3, a1, 0x0

	li.d	a1, 0xfe00000000
	or		a1, a1, a0
	ld.w	a3, a1, 0x78
	li.w	a2, ~(0x7 << 12)
	and		a3, a3, a2
	li.w	a2, 0x1000
	or		a3, a3, a2
	st.w	a3, a1, 0x78
	li.w	a2, 0x11000000
	st.w	a2, a1, 0x10

	li.d	a1, 0x11000000

	li.w	a2, ~((0x7 << 18) | (0x7 << 2))
	ld.w	a3, a1, 0x54
	and		a3, a3, a2
	st.w	a3, a1, 0x54

	ld.w	a3, a1, 0x58
	and		a3, a3, a2
	st.w	a3, a1, 0x58

	li.d	a2, 0xff204f
	st.w	a2, a1, 0x0

	jirl	zero, ra, 0
ENDPROC(ls2k_pcie0_port_conf)


ENTRY(ls2k_pcie1_port_conf)
	slli.w	a0, a0, LS_PCIE_CONF_HDR_DEV_NUM_OFFSET
	li.d	a1, 0xfe0800000c
	or		a1, a1, a0

	li.w	a2, 0xfff9ffff
	ld.w	a3, a1, 0x0
	and		a3, a3, a2
	li.w	a2, 0x20000
	or		a3, a3, a2
	st.w	a3, a1, 0x0

	li.d	a1, 0xfe0700001c
	or		a1, a1, a0
	ld.w	a3, a1, 0x0
	li.w	a2, (0x1 << 26)
	or		a3, a3, a2
	st.w	a3, a1, 0x0

	li.d	a1, 0xfe00000000
	or		a1, a1, a0
	ld.w	a3, a1, 0x78
	li.w	a2, ~(0x7 << 12)
	and		a3, a3, a2
	li.w	a2, 0x1000
	or		a3, a3, a2
	st.w	a3, a1, 0x78
	li.w	a2, 0x10000000
	st.w	a2, a1, 0x10

	li.d	a1, 0x10000000

	li.w	a2, ~((0x7 << 18) | (0x7 << 2))
	ld.w	a3, a1, 0x54
	and		a3, a3, a2
	st.w	a3, a1, 0x54

	ld.w	a3, a1, 0x58
	and		a3, a3, a2
	st.w	a3, a1, 0x58

	li.d	a2, 0xff204f
	st.w	a2, a1, 0x0

	jirl	zero, ra, 0
ENDPROC(ls2k_pcie1_port_conf)


// Note: the Address mapping window setting up already.
// 		 so, use the Mapped address 0x8xxx... or 0x9xxx...
/*
 * Simple character printing routine used before full initialization
 */
// #define UART_REF_CLK	100000000
#define UART_REF_CLK	125000000	//125MHz
#define UART_DIV_HI	(((UART_REF_CLK + (115200*8)) / (115200*16)) >> 8)
#define UART_DIV_LO	(((UART_REF_CLK + (115200*8)) / (115200*16)) & 0xff)
ENTRY(init_serial)
	or     a4, ra, zero

	li.d	a0, CONSOLE_BASE_ADDR
	li.w	a1, 0x80
	st.b	a1, a0, 3

	li.w	a1, UART_DIV_HI
	st.b	a1, a0, 1
	li.w	a1, UART_DIV_LO
	st.b	a1, a0, 0
	li.w	a1, 3
	st.b	a1, a0, 3

	li.w	a1, 71
	st.b	a1, a0, 2

	or      ra, a4, zero
	jirl	zero, ra, 0
ENDPROC(init_serial)


ENTRY(ram_init)
	or	s1, ra, zero

#if !defined(CONFIG_SPL) || defined(CONFIG_SPL_BUILD)
#ifdef DBG_ASM
	PRINTSTR("\r\nlock scache for early stack: ")
#endif
	// li.d	a0, LOCK_CACHE_BASE
	// bl		printhex64
	// PRINTSTR(" - ")
	// li.d	a0, LOCK_CACHE_BASE + LOCK_CACHE_SIZE
	// bl		printhex64

	li.d	t0, LS_SCACHE_LOCK_WIN0_BASE
	li.d	t1, ~(LOCK_CACHE_SIZE - 1)
	st.d	t1, t0, 0x40
	li.d	t1, (LOCK_CACHE_BASE & 0xffffffffffff) | (1 << 63)
	st.d	t1, t0, 0x0
#ifdef DBG_ASM
	PRINTSTR("\r\nLock Scache Done.\r\n")
#endif
#endif

#if defined(CONFIG_SPL_BUILD)
	// copy spl code to locked scache
#ifdef DBG_ASM
	PRINTSTR("\r\nCopy spl code to locked scache.\r\n")
#endif
	li.d	t0, PHYS_TO_UNCACHED(BOOT_SPACE_BASE)
	la		t1, __text_start
	la		t2, __image_copy_end
1:
	ld.w	t3, t0, 0
	st.w	t3, t1, 0
	addi.d	t0, t0, 4
	addi.d	t1, t1, 4
	blt		t1, t2, 1b

	// clear bss
	la		t0, __bss_start
	la		t1, __bss_end
2:
	st.w	zero, t0, 0
	addi.d	t0, t0, 4
	blt		t0, t1, 2b

	// jump to cache
#ifdef DBG_ASM
	PRINTSTR("\r\nJump to cache.\r\n")
#endif
	la		t0, jump_cache
	jirl	zero, t0, 0
jump_cache:

	li.d	a0, LOCK_CACHE_BASE + LOCK_CACHE_SIZE

#elif !defined(CONFIG_SPL)
	li.d	t0, LOCK_CACHE_BASE + LOCK_CACHE_SIZE
	or		sp, t0, zero

	// PRINTSTR("jump to ddr_init\r\n");
	la		t8, ddr_init
	jirl	ra, t8, 0

	// PRINTSTR("unlock scache\r\n")
	// unlock scache
	li.d	t0, LS_SCACHE_LOCK_WIN0_BASE
	st.d	zero, t0, 0x40
	st.d	zero, t0, 0
	li.d	t0, LOCK_CACHE_BASE
	li.d	t1, LOCK_CACHE_BASE + LOCK_CACHE_SIZE
2:
	cacop	0x13, t0, 0
	addi.d	t0, t0, 0x40
	blt		t0, t1, 2b

	// return the sp addr.
	li.d	a0, CONFIG_SYS_INIT_SP_ADDR
#else
	li.d	a0, CONFIG_SYS_INIT_SP_ADDR
#endif

	or		ra, s1, zero
	jirl	zero, ra, 0
ENDPROC(ram_init)


/******************************************************
 *used: a0 - char to print, a1, a2
 ******************************************************/
ENTRY(printchar)
	li.d	a1, CONSOLE_BASE_ADDR
1:
	ld.bu	a2, a1, 0x5
	andi	a2, a2, 0x20
	beqz	a2, 1b

	st.b	a0, a1, 0

	jirl	zero, ra, 0
ENDPROC(printchar)

ENTRY(update_slave_core)
	li.d	t2, LS_CORE1_IPISR

	la	t0, slave_main_call
	st.d	t0, t2, FN_OFF

	// wait a while
	li.d	t0, 20000000
1:
	addi.d	t0, t0, -1
	bnez	t0, 1b

	jirl	zero, ra, 0
ENDPROC(update_slave_core)


/****************** slave core *********************/
ENTRY(clear_mailbox)
	csrrd   t0, LOONGARCH_CSR_CPUNUM

	andi    t0, t0, CSR_CPUNUM_CID
	andi    t1, t0, 0x3
	slli.d  t1, t1, 0x8
	andi    t2, t0, 0xc
	slli.d  t2, t2, 42
	or      t1, t2, t1
	li.d    t2, LS_CORE0_IPISR
	or      t1, t1, t2
	st.d    zero, t1, FN_OFF
	st.d    zero, t1, SP_OFF
	st.d    zero, t1, GP_OFF
	st.d    zero, t1, A1_OFF

	/*
	 * 原本 这里没有返回指令 会导致继续往向下走
	 * 那么 下面那里 有一个 jirl	zero, t0, 0
	 * 其中 t0 是根据当前 ra + 12 来的
	 * 所以 就会让CPU 开始 乱跳
	 * 所以 这里就需要 返回指令 还有 调用 时要对 ra 做保护
	 */
	jirl	zero, ra, 0
ENDPROC(clear_mailbox)

slave_main:
	/* jumping to cached address */
	li.d	t3, CACHED_MEMORY_ADDR
	bl	1f
1:
	addi.d	t0, ra, 12
	or		t0, t3, t0
	jirl	zero, t0, 0
	/* now pc run to 0x90xxxxxxxxxxxxxx */

	/* a0, ap own cpu number
	 * t2, bsp mail box address
	 */
	// t3 来对 ra 做保护
	move    t3, ra
	bl clear_mailbox
	move    ra, t3

	/* t1 store mailbox base so don't changing it */

slave_main_call:
	st.d    zero, t1, FN_OFF
waitforinit:
	li.d      t0, 0x100

idle1000:
	addi.w  t0, t0, -1
	bnez    t0, idle1000
	/*csr finally filled the low 32 bits*/
	ld.d    t0, t1, FN_OFF
	beqz    t0, waitforinit

	ld.d    t0, t1, FN_OFF

	or      ra, t0,zero

	li.d     t3, CACHED_MEMORY_ADDR

	ld.d    t0, t1, SP_OFF
	or      t0, t0, t3
	or      sp, t0, zero

	ld.d    t0, t1, GP_OFF
	or      t0, t0, t3
	or      tp, t0, zero

	st.d    zero, t1, FN_OFF

	# slave core jump to kernel, byebye
	jirl    zero, ra, 0x0
//end slave_main
