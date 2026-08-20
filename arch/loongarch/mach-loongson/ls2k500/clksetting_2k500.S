
#ifndef _KERNEL
#define _KERNEL
#endif

#include <config.h>
#include <asm/loongarch.h>

/* NODE PLL */
// #define CORE_FREQ	500					//CPU 500Mhz
#define NODE_REFC	4
#if CORE_FREQ <= 700
#define NODE_DIV	4
#else
#define NODE_DIV	2
#endif
#define NODE_LOOPC	(CORE_FREQ*NODE_REFC*NODE_DIV/REF_FREQ)

/* DDR PLL */
// #define DDR_FREQ	400					//MEM 400~600Mhz
#define DDR_REFC	4

#if DDR_FREQ != 500
// #define DDR_DIV		2
#define DDR_DIV		(1200 / DDR_FREQ)
#define DDR_LOOPC	48  // 1200M/25M=48 to apply HDA 24MHz 1.2GHz
// #define DDR_LOOPC	(DDR_FREQ*DDR_REFC*DDR_DIV/REF_FREQ)
#else
#define DDR_DIV		3
#define DDR_LOOPC	(DDR_FREQ*DDR_REFC*DDR_DIV/REF_FREQ)
#endif


#define HDA_FREQ	24 	//HDA 24MHz, NO changed.
#define HDA_DIV		50
// #define HDA_DIV		((DDR_FREQ*DDR_DIV+12)/HDA_FREQ)

// #define NET_FREQ	200
// #define NETWORK_DIV	((DDR_FREQ*DDR_DIV+100)/NET_FREQ)
#define NETWORK_DIV	(1200 / NET_FREQ)

/* SOC PLL */
#define GMAC_FREQ	125					//GMAC 125MHz
#define GPU_FREQ	200					//GPU 200~300MHz
// #define SB_FREQ		100					//SB 100~200MHz
#define SOC_REFC	4
#define GMAC_DIV	16
#define SOC_LOOPC	(GMAC_FREQ*SOC_REFC*GMAC_DIV/REF_FREQ)
#define GPU_DIV		((GMAC_FREQ*GMAC_DIV+GPU_FREQ/2)/GPU_FREQ)
#define SB_DIV		((GMAC_FREQ*GMAC_DIV+SB_FREQ/2)/SB_FREQ)

#if ((DDR_LOOPC > 255) | (NODE_LOOPC > 255) | (SOC_LOOPC > 255))
#error PLL LOOPC overflow
#endif

/* PIX PLL */
#define PIX0_LOOPC	109					//100~200MHz
#define PIX0_REFC	5
#define PIX0_DIV	20

#define PIX1_LOOPC	109
#define PIX1_REFC	5
#define PIX1_DIV	20

#define SEL_PLL0	(0x1)
#define SEL_PLL1	(0x2)
#define SEL_PLL2	(0x4)
#define PLL_L1_ENA	(0x1 << 3)
#define PLL_L1_PD_PLL	(0x1 << 5)
#define PLL_L1_LOCKED	(0x1 << 7)

/*
 * 注意手册上写的 3.5.2 软件配置
 * 第二步和第三步是一起的，也就是代码中
 * li.w	t1, (NODE_DIV << 24) | (NODE_LOOPC << 16) | (NODE_REFC << 8)
 * 没有管 pd_pll 会被设置成 0 的原因
 */

//////////// Node ////////////
	li.d	t0, LS_NODE_PLL_L

	/* pd_pll 1 */
	li.w	t1, PLL_L1_PD_PLL	//power down pll L1 first
	st.w	t1, t0, 0

	/* 除了 pll_sel_* [2:0] 设置为0 pll_soft_set (3) 设置为0 其他的寄存器按需设置*/
	/*  pd_pll 0 */
	li.w	t1, (NODE_DIV << 24) | (NODE_LOOPC << 16) | (NODE_REFC << 8)
	st.w	t1, t0, 0

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
	ori	a0, a0, SEL_PLL0
	st.w	a0, t0, 0

//////////// DDR ////////////
	li.d	t0, LS_DDR_PLL_L

	/* pd_pll 1 */
	li.w	t1, PLL_L1_PD_PLL	//power down pll L1 first
	st.w	t1, t0, 0

	/* 除了 pll_sel_* [2:0] 设置为0 pll_soft_set (3) 设置为0 其他的寄存器按需设置*/
	/*  pd_pll 0 */
	li.w	t1, (DDR_DIV << 24) | (DDR_LOOPC << 16) | (DDR_REFC << 8)
	li.w	t2, (HDA_DIV << 8) | (NETWORK_DIV)
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

//////////// SOC ////////////
	li.d	t0, LS_SOC_PLL_L

	/* pd_pll 1 */
	li.w	t1, PLL_L1_PD_PLL	//power down pll L1 first
	st.w	t1, t0, 0

	/* 除了 pll_sel_* [2:0] 设置为0 pll_soft_set (3) 设置为0 其他的寄存器按需设置*/
	/*  pd_pll 0 */
	li.w	t1, (GPU_DIV << 24) | (SOC_LOOPC << 16) | (SOC_REFC << 8)
	li.w	t2, (GMAC_DIV << 8) | (SB_DIV)
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

//////////// PIX0 ////////////

	li.d	t0, LS_PIX0_PLL_L		//pll_pix0

	/* pd_pll 1 */
	li.w	t1, PLL_L1_PD_PLL	//power down pll L1 first
	st.w	t1, t0, 0

	/* 除了 pll_sel_* [2:0] 设置为0 pll_soft_set (3) 设置为0 其他的寄存器按需设置*/
	/*  pd_pll 0 */
	li.w	t1, (PIX0_DIV << 24) | (PIX0_REFC << 8) | (PIX0_LOOPC << 16)
	st.w	t1, t0, 0

	/* 其他信号不变 pll_soft_set (3) 信号设置为 1 */
	ori	t1, t1, PLL_L1_ENA
	st.w	t1, t0, 0

	/* 等待 pll_lock (7) 锁定 也就是变成 1 */
21:
	ld.w	a0, t0, 0
	li.w	a1, PLL_L1_LOCKED
	and	a0, a0, a1
	beqz	a0, 21b
	nop

	/* pll_sel_* [2:0] 设置为1 */
	ld.w	a0, t0, 0
	ori	a0, a0, SEL_PLL0
	st.w	a0, t0, 0

//////////// PIX1 ////////////

	li.d	t0, LS_PIX1_PLL_L		//pll_pix1

	/* pd_pll 1 */
	li.w	t1, PLL_L1_PD_PLL	//power down pll L1 first
	st.w	t1, t0, 0

	/* 除了 pll_sel_* [2:0] 设置为0 pll_soft_set (3) 设置为0 其他的寄存器按需设置*/
	/*  pd_pll 0 */
	li.w	t1, (PIX1_DIV << 24) | (PIX1_REFC << 8) | (PIX1_LOOPC << 16)
	st.w	t1, t0, 0

	/* 其他信号不变 pll_soft_set (3) 信号设置为 1 */
	ori	t1, t1, PLL_L1_ENA
	st.w	t1, t0, 0

	/* 等待 pll_lock (7) 锁定 也就是变成 1 */
21:
	ld.w	a0, t0, 0
	li.w	a1, PLL_L1_LOCKED
	and	a0, a0, a1
	beqz	a0, 21b
	nop

	/* pll_sel_* [2:0] 设置为1 */
	ld.w	a0, t0, 0
	ori	a0, a0, SEL_PLL0
	st.w	a0, t0, 0

