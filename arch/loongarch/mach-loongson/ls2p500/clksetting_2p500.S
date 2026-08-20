
#ifndef _KERNEL
#define _KERNEL
#endif

#include <config.h>
#include <asm/loongarch.h>

/* SYS PLL */
#define SYS_HCLK	2400
//#define CORE_FREQ	800				//CPU 600Mhz
#define LA132_FREQ	200				//
//#define SOC_FREQ	200				//
#define SYS_REFC	4
#define SYS_LOOPC	(SYS_HCLK/(REF_FREQ/SYS_REFC))		/*1.2G~3.2G   2.4Gused*/
#define NODE_DIV	(SYS_HCLK/CORE_FREQ)
#define LA132_DIV	(SYS_HCLK/LA132_FREQ)
#define SOC_DIV		(SYS_HCLK/SOC_FREQ)

/* DDR PLL */
//#define DDR_FREQ		400				//MEM 400~600Mhz
#define DDR_HCLK		2400
//#define NETWORK_FREQ	300				//NETWORK 300~400Mhz
#define IMG_FREQ		200				//IMAGE 200~300Mhz
#define DDR_REFC		4
#define DDR_LOOPC	(DDR_HCLK/(REF_FREQ/DDR_REFC))
#define DDR_DIV		(DDR_HCLK/DDR_FREQ)
#define NETWORK_DIV	(DDR_HCLK/NETWORK_FREQ)	//NETWORK 300~400MHz
#define IMG_DIV		(DDR_HCLK/IMG_FREQ)		//IMG 200~300MHz

/* VID PLL */
#define VID_HCLK	2000
#define PRVID_FREQ	250					//PRVID 200~300MHz
#define SCVID_FREQ	250					//SCVID 200~300MHz
#define GMAC_FREQ	125					//GMAC  125MHz
#define VID_REFC	4
#define VID_LOOPC	(VID_HCLK/(REF_FREQ/VID_REFC))
#define PRVID_DIV	(VID_HCLK/PRVID_FREQ)
#define SCVID_DIV	(VID_HCLK/SCVID_FREQ)
#define GMACBP_DIV	(VID_HCLK/GMAC_FREQ)

#if ((DDR_LOOPC > 255) | (SYS_LOOPC > 255) | (VID_LOOPC > 255))
PLL LOOPC overflow
#endif

#define SEL_PLL0	(0x1)
#define SEL_PLL1	(0x2)
#define SEL_PLL2	(0x4)
#define PLL_L1_ENA	(0x1 << 3)
#define PLL_L1_PD_PLL	(0x1 << 5)
#define PLL_L1_LOCKED	(0x1 << 7)


	//usb and boot 1/2 of SOC freq
	li.d	t0, LS_FREQ_SCALE0
	ld.w	t1, t0, 0
	//usb boot
	li.w    t2, ~((0xf << 12) | (0xf << 4))
	and     t1, t1, t2
	li.w    t2, ((0x3 << 12) | (0x3 << 4))
	or	t1, t1, t2
	st.w	t1, t0, 0

	//gmac clksel GMACBP-PLL
	li.d	t0, LS_CLK_ENABLE
	ld.w	t1, t0, 0
	li.w    t2, ~(0x1 << 31)
	and     t1, t1, t2
	st.w	t1, t0, 0

	//scdev and prdev freq 1/2 of ls132
	li.d	t0, LS_FREQ_SCALE1
	ld.w	t1, t0, 0
	li.w    t2, ~(0xf << 8) //prdev
	and     t1, t1, t2
	li.w    t2, (0x3 << 8)
	or	t1, t1, t2
	st.w	t1, t0, 0

	li.d	t0, LS_FREQ_SCALE2
	ld.w	t1, t0, 0
	li.w    t2, ~(0xf << 8) //scdev
	and     t1, t1, t2
	li.w    t2, (0x3 << 8)
	or	t1, t1, t2
	st.w	t1, t0, 0

/*
 * 注意手册上写的 3.4.2 软件配置
 * 第二步和第三步是一起的，也就是代码中
 * li.w	t1, (NODE_DIV << 24) | (NODE_LOOPC << 16) | (NODE_REFC << 8)
 * 没有管 pd_pll 会被设置成 0 的原因
 */

//////////// Node ////////////
	li.d	t0, LS_SYS_PLL_L

	/* pd_pll 1 */
	li.d	t1, PLL_L1_PD_PLL	//power down pll L1 first
	st.w	t1, t0, 0

	/* 除了 pll_sel_* [2:0] 设置为0 pll_soft_set (3) 设置为0 其他的寄存器按需设置*/
	/*  pd_pll 0 */
	li.w	t1, (NODE_DIV << 24) | (SYS_LOOPC << 16) | (SYS_REFC << 8)
	st.w	t1, t0, 0
	li.w	t2, (SOC_DIV << 8) | (LA132_DIV)
	st.w	t2, t0, 0x4

	/* 其他信号不变 pll_soft_set (3) 信号设置为 1 */
	ori	t1, t1, PLL_L1_ENA
	st.w	t1, t0, 0

	/* 等待 pll_lock (7) 锁定 也就是变成 1 */
11:
	ld.w	a0, t0, 0
	li.w	a1, PLL_L1_LOCKED
	and	a0, a1, a0
	beqz	a0, 11b //wait_locked_sys
	nop

	/* pll_sel_* [2:0] 设置为1 */
	ld.w	a0, t0, 0
	ori	a0, a0, SEL_PLL0 | SEL_PLL1 | SEL_PLL2
	st.w	a0, t0, 0

//////////// DDR ////////////
	li.d	t0, LS_DDR_PLL_L

	/* pd_pll 1 */
	li.w	t1, PLL_L1_PD_PLL	//power down pll L1 first
	st.w	t1, t0, 0

	/* 除了 pll_sel_* [2:0] 设置为0 pll_soft_set (3) 设置为0 其他的寄存器按需设置*/
	/*  pd_pll 0 */
	li.w	t1, (DDR_DIV << 24) | (DDR_LOOPC << 16) | (DDR_REFC << 8)
	li.w	t2, (IMG_DIV << 8) | (NETWORK_DIV)
	st.w	t1, t0, 0
	st.w	t2, t0, 0x4

	/* 其他信号不变 pll_soft_set (3) 信号设置为 1 */
	ori	t1, t1, PLL_L1_ENA
	st.w	t1, t0, 0

	/* 等待 pll_lock (7) 锁定 也就是变成 1 */
21:
	ld.w	a0, t0, 0
	li.w	a1, PLL_L1_LOCKED
	and	a0, a0, a1
	beqz	a0, 21b //wait_locked_ddr
	nop

	/* pll_sel_* [2:0] 设置为1 */
	ld.w	a0, t0, 0
	ori	a0, a0, SEL_PLL0 | SEL_PLL1 | SEL_PLL2
	st.w	a0, t0, 0

//////////// VID ////////////
	li.d	t0, LS_VID_PLL_L
	li.w	t1, PLL_L1_PD_PLL	//power down pll first
	st.w	t1, t0, 0
	li.w	t1, (PRVID_DIV << 24) | (VID_LOOPC << 16) | (VID_REFC << 8)
	li.w	t2, (GMACBP_DIV << 8) | (SCVID_DIV)
	st.w	t1, t0, 0
	st.w	t2, t0, 0x4
	ori	t1, t1, PLL_L1_ENA
	st.w	t1, t0, 0

21:
	ld.w	a0, t0, 0
	li.w	a1, PLL_L1_LOCKED
	and	a0, a0, a1
	beqz	a0, 21b //wait_locked_ddr

	ld.w	a0, t0, 0
	ori	a0, a0, SEL_PLL0 | SEL_PLL1 | SEL_PLL2
	st.w	a0, t0, 0
	ld.w	a0, t0, 0


