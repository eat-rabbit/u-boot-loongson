#include <config.h>
#include <linux/linkage.h>
#include <asm/regdef.h>
#include <asm/addrspace.h>
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
	or	t2, t2, t3
	st.w	t2, t1, 0x30	//enable watchdog

	li.w	t2, 0x1
	st.w	t2, t1, 0x34	//set watchdog time
.endm

.macro watchdog_close
	//disable watch DOG.
	li.d	t1, LS_RST_CNT_REG
	ld.w	t2, t1, 0x0
	li.w	t3, ~0x2
	and	t2, t2, t3
	st.w	t2, t1, 0x0
.endm

.macro beep_off
	/* enable gpio34 */
	li.d	t1, GPIO_CFG
	ld.w	t0, t1, 0x0
	li.w	t2, ~(0x7 << (GPIO_BASE_OFFSET*4))
	and	t0, t0, t2
	st.w	t0, t1, 0x0

	/* enable gpio34 output */
	li.d	t1, GPIO_OEN
	ld.w	t0, t1, 0x0
	li.w	t2, ~(0x1 << GPIO_BASE_OFFSET)
	and	t0, t0, t2
	st.w	t0, t1, 0x0

	/* set gpio34 high */
	li.d	t1, GPIO_OUT
	ld.w	t0, t1, 0x0
	li.w	t2, (0x1 << GPIO_BASE_OFFSET)
	or	t0, t0, t2
	st.w	t0, t1, 0x0
.endm


.macro beep_on
	/* enable gpio34 */
	li.d	t1, GPIO_CFG
	ld.w	t0, t1, 0x0
	li.w	t2, ~(0x7 << (GPIO_BASE_OFFSET*4))
	and		t0, t0, t2
	st.w	t0, t1, 0x0

	/* enable gpio34 output */
	li.d	t1, GPIO_OEN
	ld.w	t0, t1, 0x0
	li.w	t2, ~(0x1 << GPIO_BASE_OFFSET)
	and		t0, t0, t2
	st.w	t0, t1, 0x0

	/* set gpio34 low */
	li.d	t1, GPIO_OUT
	ld.w	t0, t1, 0x0
	li.w	t2, ~(0x1 << GPIO_BASE_OFFSET)
	and		t0, t0, t2
	st.w	t0, t1, 0x0
.endm

ENTRY(lowlevel_init)
    or     a4, ra, zero

	// 窗口不命中时，由内存控制器给出响应，防止CPU卡住。
	/*default mem config space*/
	li.d	t0, LS_GENERAL_CFG0
	ld.w	t1, t0, 0x0
	li.w	t2, ~0x3
	and	t1, t1, t2
	li.w	t2, (0x1 << 1)
	or	t1, t1, t2
	st.w	t1, t0, 0x0

	/*config resume coherent*/
	/*[51:45] en [59:53] coherent*/
	/*0:cpu    1:usb     2:gmac1    3:resume(gmac0 otg)   4:imp(jpeg jbig)
		5:apb-dma(sdio nand)  6:pr-dma   7:sc-dma*/
	li.d	t0, LS_GENERAL_CFG1
	ld.w	t1, t0, 0x4
	li.w	t2, ((0x7f << 13) | (0x7f << 21))
	or	t1, t1, t2
	st.w	t1, t0, 0x0

	/*gmac1_mii_sel  rgmii mode*/
	li.d	t0, LS_GENERAL_CFG0
	ld.w	t1, t0, 0x0
	li.w	t2, ~(0x1 << 14)
	and	t1, t1, t2
	st.w	t1, t0, 0x0

# 	watchdog_close

# #if defined(GPIO_CFG) && defined(GPIO_OEN) && defined(GPIO_BASE_OFFSET)
# 	beep_on
# 	li.w	t0, 0x600
# 1:
# 	addi.d	t0, t0, -1
# 	bnez	t0, 1b
# 	beep_off
# #endif

 	/* spi speedup */
 	li.d	t0, LS_SPI0_BASE
 	li.w	t1, 0x47
 	st.b	t1, t0, 0x4

# //#define SPI_QUAD_IO
# #ifdef	SPI_QUAD_IO
# 	/* spi quad_io */
# 	li.w	t1, 0xb
# 	st.b	t1, t0, 0x6
# spi_quad_io:
# 	ld.bu	t2, t0, 0x6
# 	bne	t2, t1, spi_quad_io
# #endif

	/* select uart0 */
	li.d	t0, LS_MAIN_GPIO_PINMUX_CFG
	ld.w	t1, t0, 0x0
	li.w	t2, 0xf
	or	t1, t1, t2
	st.w	t1, t0, 0x0

	#include "clksetting_2p500.S"

	//delay a while
	li.d	t1, 0x100
1:
	addi.d	t1, t1, -1
	bnez	t1, 1b
	nop

	or	ra, a4, zero
	jirl	zero, ra, 0
ENDPROC(lowlevel_init)


// Note: the Address mapping window setting up already.
// 		 so, use the Mapped address 0x8xxx... or 0x9xxx...
/*
 * Simple character printing routine used before full initialization
 */
#define UART_REF_CLK	(SOC_FREQ*1000000)
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
	li.w	a1, 3	#CFCR_8BITS
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
#ifdef DBG_ASM
	PRINTSTR("\r\nCopy spl code to locked scache...\r\n");
#endif
	// copy spl code to locked scache
	li.d	t0, BOOT_SPACE_BASE_UNCACHED
	la		t1, __text_start
	la		t2, __image_copy_end
#ifdef DBG_ASM
	PRINTSTR("\r\ncopy text and rodata section to lock cache start....\r\n")
#endif
1:
	ld.w	t3, t0, 0
	st.w	t3, t1, 0
	addi.d	t0, t0, 4
	addi.d	t1, t1, 4
	blt		t1, t2, 1b

#ifdef DBG_ASM
	PRINTSTR("copy text and rodata section to lock cache done.\r\n")
#endif
	// clear bss
	la	t0, __bss_start
	la	t1, __bss_end
2:
	st.w	zero, t0, 0
	addi.d	t0, t0, 4
	blt	t0, t1, 2b

	// jump to cache
	# la	t0, jump_cache
	# jirl	zero, t0, 0
jump_cache:

	li.d	a0, LOCK_CACHE_BASE + LOCK_CACHE_SIZE

#elif !defined(CONFIG_SPL)
	li.d	t0, LOCK_CACHE_BASE + LOCK_CACHE_SIZE
	or		sp, t0, zero
#ifdef DBG_ASM
	PRINTSTR("\r\nJump to ddr_init.......\r\n");
#endif
	la		t8, ddr_init
	jirl	ra, t8, 0

# #ifdef DBG_ASM
# 	PRINTSTR("\r\nUnlock scache........\r\n")
# #endif
# 	// unlock scache
# 	li.d	t0, LS_SCACHE_LOCK_WIN0_BASE
# 	st.d	zero, t0, 0x40
# 	st.d	zero, t0, 0
# 	li.d	t0, LOCK_CACHE_BASE
# 	li.d	t1, LOCK_CACHE_BASE + LOCK_CACHE_SIZE
# 2:
# 	cacop	0x13, t0, 0
# 	addi.d	t0, t0, 0x40
# 	blt	t0, t1, 2b

# 	// return the sp addr.
# 	li.d	a0, CONFIG_SYS_INIT_SP_ADDR
#else
	li.d	a0, CONFIG_SYS_INIT_SP_ADDR
#endif

	or	ra, s1, zero
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
