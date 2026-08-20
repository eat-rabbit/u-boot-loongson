/*
 * writereg(SHIFTW(v) & MASK)
 * val = SHIFTR(readreg() & MASK)
 *
 * 0011 -> 1100 -> 0100
 * 0001 <- 0100 <- 1100
 *
 * w*(sw*m)
 * sr*(r*m)
 */
#include <common.h>
#include <malloc.h>
#include <dm.h>
#include <mmc.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/unaligned.h>
#include <errno.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <wait_bit.h>
#include <div64.h>

/* Registers */
#define REG_SDICON	0x00
#define REG_SDIPRE	0x04
#define REG_SDICMDARG	0x08
#define REG_SDICMDCON	0x0c
#define REG_SDICMDSTA	0x10
#define REG_SDIRSP0	0x14
#define REG_SDIRSP1	0x18
#define REG_SDIRSP2	0x1C
#define REG_SDIRSP3	0x20
#define REG_SDIDTIMER	0x24
#define REG_SDIBSIZE	0x28
#define REG_SDIDATCON	0x2C
#define REG_SDIDATCNT	0x30
#define REG_SDIDATSTA	0x34
#define REG_SDIFSTA	0x38
#define REG_SDIINTMSK	0x3C
#define REG_SDIWRDAT	0x40
#define REG_SDISTAADD0	0x44
#define REG_SDISTAADD1	0x48
#define REG_SDISTAADD2	0x4c
#define REG_SDISTAADD3	0x50
#define REG_SDISTAADD4	0x54
#define REG_SDISTAADD5	0x58
#define REG_SDISTAADD6	0x5c
#define REG_SDISTAADD7	0x60
#define REG_SDIINTEN	0x64
#define REG_SDIDLLMVAL	0xf0
#define REG_SDIDLLCON	0xf4
#define REG_SDICLKDELAY	0xf8
#define REG_SDISESEL	0xfc

#define SHIFTW(_x, _s) ((_x) << (_s))
#define SHIFTR(_x, _s) ((_x) >> (_s))

// SDICON - RESET and Clock Enable
#define BIT_SDICON_RESET	BIT(8)
#define BIT_SDICON_CLKEN	BIT(0)
#define BIT_SDICON_UNKWN	BIT(1)

// SDIPRE - Prescale
#define BIT_SDIPRE_REVCLK	BIT(31)
#define SHIFT_SDIPRE_SCALE	(0)
#define MASK_SDIPRE_SCALE	SHIFTW(0xff, SHIFT_SDIPRE_SCALE)

// SDICMDCON - Command & CommandControl
#define SHIFT_SDICMDCON_ABORTNUM	(15)
#define MASK_SDICMDCON_ABORTNUM	SHIFTW(0x7, SHIFT_SDICMDCON_ABORTNUM)
#define BIT_SDICMDCON_EMMCEN	BIT(14)
#define BIT_SDICMDCON_CHECKCRC	BIT(13)
#define BIT_SDICMDCON_AUTOSTOPEN	BIT(12)
#define BIT_SDICMDCON_LONGRSP	BIT(10)
#define BIT_SDICMDCON_WAITRSP	BIT(9)
#define BIT_SDICMDCON_CMDSTART	BIT(8)
#define BIT_SDICMDCON_SENDERHOST  BIT(6)
#define SHIFT_SDICMDCON_CMDIDX	(0)
#define MASK_SDICMDCON_CMDIDX	SHIFTW(0xff, SHIFT_SDICMDCON_CMDIDX)

#define SDICMDCON_CMD6DATA	BIT(18)

// SDICMDSTA - Command Status SNDEND->RSPEND->FIN
#define BIT_SDICMDSTA_FIN	BIT(14)
#define BIT_SDICMDSTA_AUTOSTOP	BIT(13)
#define BIT_SDICMDSTA_RSPCRC	BIT(12)
#define BIT_SDICMDSTA_SNDEND	BIT(11)
#define BIT_SDICMDSTA_TIMEOUT	BIT(10)
#define BIT_SDICMDSTA_RSPEND	BIT(9)
#define BIT_SDICMDSTA_SNDON	BIT(8)
#define SHIFT_SDICMDSTA_RSPIDX	(0)
#define MASK_SDICMDSTA_RSPIDX	SHIFTW(0xff, SHIFT_SDICMDSTA_RSPIDX)

// SDIDTIMER - DATA Timeout
#define SHIFT_SDIDTIMER_T	(0)
#define MASK_SDIDTIMER_T	SHIFTW(0xffffff, SHIFT_SDIDTIMER_T)

// SDIBSIZE  - Block Size
#define SHIFT_SDIBSIZE_B	(0)
#define MASK_SDIBSIZE_B	SHIFTW(0xfff, SHIFT_SDIBSIZE_B)

// SDIDATCON - Data Control
#define BIT_SDIDATCON_WIDEMD8B	BIT(26)
#define BIT_SDIDATCON_RSMRW	BIT(20)
#define BIT_SDIDATCON_RSMIO	BIT(19)
#define BIT_SDIDATCON_SSPDIO	BIT(18)
#define BIT_SDIDATCON_WAITR	BIT(17)
#define BIT_SDIDATCON_WIDEMD	BIT(16)
#define BIT_SDIDATCON_DMAEN	BIT(15)
#define BIT_SDIDATCON_DSTART	BIT(14)
#define SHIFT_SDIDATCON_BNUM	(0)
#define MASK_SDIDATCON_BNUM	SHIFTW(0xfff, SHIFT_SDIDATCON_BNUM)

// SDIDATCNT - Data Transfered Count
#define SHIFT_SDIDATCNT_BSIZE	(12)
#define MASK_SDIDATCNT_BSIZE	SHIFTW(0xfff, SHIFT_SDIDATCNT_BSIZE)
#define SHIFT_SDIDATCNT_BNUM	(0)
#define MASK_SDIDATCNT_BNUM	SHIFTW(0xfff, SHIFT_SDIDATCNT_BNUM)

// SDIDATSTA - Data Status
#define BIT_SDIDSTA_SSPDON	BIT(16)
#define BIT_SDIDSTA_RSTSSPD	BIT(15)
#define BIT_SDIDSTA_R1BTIMEOUT	BIT(14)
#define BIT_SDIDSTA_DATASTART	BIT(13)
#define BIT_SDIDSTA_R1BFIN	BIT(12)
#define BIT_SDIDSTA_AUTOSTOP	BIT(11)
#define BIT_SDIDSTA_WAITR	BIT(9)
#define BIT_SDIDSTA_INTERRUPT	BIT(8)
#define BIT_SDIDSTA_SNDCRC	BIT(7)
#define BIT_SDIDSTA_RCVCRC	BIT(6)
#define BIT_SDIDSTA_TIMEOUT	BIT(5)
#define BIT_SDIDSTA_DATAFIN	BIT(4)
#define BIT_SDIDSTA_BUSYFIN	BIT(3)
#define BIT_SDIDSTA_PROGERR	BIT(2)
#define BIT_SDIDSTA_TXON	BIT(1)
#define BIT_SDIDSTA_RXON	BIT(0)

// SDIFSTA - FIFO Status
#define BIT_SDIFSTA_TXFULL	BIT(11)
#define BIT_SDIFSTA_TXEMPT	BIT(10)
#define BIT_SDIFSTA_RXFULL	BIT(8)
#define BIT_SDIFSTA_RXEMPT	BIT(7)

// SDIINTMSK/SDIINTEN - Interrupts & Interrupts Enable
#define BIT_SDIINT_R1BFIN	BIT(9)
#define BIT_SDIINT_CMDRSPCRC	BIT(8)
#define BIT_SDIINT_CMDTIMEOUT	BIT(7)
#define BIT_SDIINT_CMDFIN	BIT(6)
#define BIT_SDIINT_EMMC	BIT(5)
#define BIT_SDIINT_PROGERR	BIT(4)
#define BIT_SDIINT_DATASNDCRC	BIT(3)
#define BIT_SDIINT_DATARCVCRC	BIT(2)
#define BIT_SDIINT_DATATIMEOUT	BIT(1)
#define BIT_SDIINT_DATAFIN	BIT(0)
#define SDIINT_ALL \
	(BIT_SDIINT_R1BFIN | BIT_SDIINT_CMDRSPCRC | BIT_SDIINT_CMDTIMEOUT |\
	 BIT_SDIINT_CMDFIN | BIT_SDIINT_EMMC | BIT_SDIINT_PROGERR |\
	 BIT_SDIINT_DATASNDCRC | BIT_SDIINT_DATARCVCRC | BIT_SDIINT_DATATIMEOUT |\
	 BIT_SDIINT_DATAFIN)

// SDIDLLMVAL - DLL Master
#define BIT_SDIDLLMVAL_INITDONE	BIT(8)
#define SHIFT_SDIDLLMVAL_V	(0)
#define MASK_SDIDLLMVAL_V	SHIFTW(0xff, SHIFT_SDIDLLMVAL_V)

// SDIDLLCON - DLL Control
#define BIT_SDIDLLCON_RSYNCRD	BIT(29)
#define BIT_SDIDLLCON_BYPASSRD	BIT(28)
#define BIT_SDIDLLCON_RSYNCPAD	BIT(27)
#define BIT_SDIDLLCON_BYPASSPAD	BIT(26)
#define BIT_SDIDLLCON_MINIT	BIT(25)
#define BIT_SDIDLLCON_MLOCK	BIT(24)
#define SHIFT_SDIDLLCON_MSTART	(16)
#define MASK_SDIDLLCON_MSTART	SHIFTW(0xff, SHIFT_SDIDLLCON_MSTART)
#define SHIFT_SDIDLLCON_MSTEP	(8)
#define MASK_SDIDLLCON_MSTEP	SHIFTW(0xff, SHIFT_SDIDLLCON_MSTEP)
#define SHIFT_SDIDLLCON_MINTERVAL	(0)
#define MASK_SDIDLLCON_MINTERVAL	SHIFTW(0xff, SHIFT_SDIDLLCON_MINTERVAL)

// SDICLKDELAY - DLL Delay
#define SHIFT_SDICLKDELAY_RD	(8)
#define MASK_SDICLKDELAY_RD	SHIFTW(0xff, SHIFT_SDICLKDELAY_RD)
#define SHIFT_SDICLKDELAY_PAD	(0)
#define MASK_SDICLKDELAY_PAD	SHIFTW(0xff, SHIFT_SDICLKDELAY_PAD)

// SDISESEL - Bus Type Select emmc/sdio sdr/ddr
#define BIT_SDISESEL_BUS	BIT(1)
#define BIT_SDISESEL_DATA	BIT(0)

#define SDISESEL_BUS_EMMC	(0x00)
#define SDISESEL_BUS_SDIO	(BIT_SDISESEL_BUS)
#define SDISESEL_DATA_SDR	(0x00)
#define SDISESEL_DATA_DDR	(BIT_SDISESEL_DATA)

#define MSC_CMDAT_INIT			BIT(7)

#ifdef CONFIG_MMC_TRACE
#define ls_mmc_printf printf
#else
#define ls_mmc_printf(x, ...)
#endif

struct ls_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct ls_mmc_priv {
	void __iomem *regs;
	u32 ref_clock;
};

void ls_mmc_dump_reg(void)
{
#ifdef CONFIG_MMC_TRACE
	void __iomem *regs = PHYS_TO_UNCACHED(0x14210000);
#define __DUMP_PRINTF(o,v,fmt,...) printf("+0x%02x:0x%08x["#fmt"]\n",o,v,##__VA_ARGS__)
	u32 val, offset;

	offset = REG_SDICON;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"RST(%1d)|...|CLKEN(%1d)", 
			(val & BIT_SDICON_RESET) ? 1 : 0,
			(val & BIT_SDICON_CLKEN) ? 1 : 0
		);

	offset = REG_SDIPRE;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"REVCLK(%1d)|...|PRESCALE(%d)", 
			(val & BIT_SDIPRE_REVCLK) ? 1 : 0,
			SHIFTR(val & MASK_SDIPRE_SCALE, SHIFT_SDIPRE_SCALE)
		);

	offset = REG_SDICMDARG;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"CMDARG"
		);

	offset = REG_SDICMDCON;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"ABORTNUM(%d)|EMMCEN(%1d)|CHECKCRC(%1d)|AUTOSTOP(%1d)|...|"
			"LONGRSP(%1d)|WAITRSP(%1d)|CMDSTART(%1d)|CMDIDX(%d)",
			SHIFTR(val & MASK_SDICMDCON_ABORTNUM, SHIFT_SDICMDCON_ABORTNUM),
			(val & BIT_SDICMDCON_EMMCEN) ? 1 : 0,
			(val & BIT_SDICMDCON_CHECKCRC) ? 1 : 0,
			(val & BIT_SDICMDCON_AUTOSTOPEN) ? 1 : 0,
			(val & BIT_SDICMDCON_LONGRSP) ? 1 : 0,
			(val & BIT_SDICMDCON_WAITRSP) ? 1 : 0,
			(val & BIT_SDICMDCON_CMDSTART) ? 1 : 0,
			SHIFTR(val & MASK_SDICMDCON_CMDIDX, SHIFT_SDICMDCON_CMDIDX)
		);

	offset = REG_SDICMDSTA;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"FIN(%1d)|AUTOSTOP(%1d)|RSPCRC(%1d)|SNDEND(%1d)|"
			"TIMEOUT(%1d)|RSPEND(%1d)|SNDON(%1d)|RSPIDX(%d)", 
			(val & BIT_SDICMDSTA_FIN) ? 1 : 0,
			(val & BIT_SDICMDSTA_AUTOSTOP) ? 1 : 0,
			(val & BIT_SDICMDSTA_RSPCRC) ? 1 : 0,
			(val & BIT_SDICMDSTA_SNDEND) ? 1 : 0,
			(val & BIT_SDICMDSTA_TIMEOUT) ? 1 : 0,
			(val & BIT_SDICMDSTA_RSPEND) ? 1 : 0,
			(val & BIT_SDICMDSTA_SNDON) ? 1 : 0,
			SHIFTR(val & MASK_SDICMDSTA_RSPIDX, SHIFT_SDICMDSTA_RSPIDX)
		);

	offset = REG_SDIRSP0;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"RSP0"
		);

	offset = REG_SDIRSP1;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"RSP1"
		);

	offset = REG_SDIRSP2;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"RSP2"
		);

	offset = REG_SDIRSP3;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"RSP3"
		);

	offset = REG_SDIDTIMER;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"DTIMER"
		);

	offset = REG_SDIBSIZE;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"BSIZE"
		);

	offset = REG_SDIDATCON;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"WIDE8B(%1d)|...|RSMRW(%1d)|RSMIO(%1d)|SSPDIO(%1d)|"
			"WAITR(%1d)|WIDE(%1d)|DMAEN(%1d)|DATASTART(%1d)|...|"
			"BLOCKNUM(%d)", 
			(val & BIT_SDIDATCON_WIDEMD8B) ? 1 : 0,
			(val & BIT_SDIDATCON_RSMRW) ? 1 : 0,
			(val & BIT_SDIDATCON_RSMIO) ? 1 : 0,
			(val & BIT_SDIDATCON_SSPDIO) ? 1 : 0,
			(val & BIT_SDIDATCON_WAITR) ? 1 : 0,
			(val & BIT_SDIDATCON_WIDEMD) ? 1 : 0,
			(val & BIT_SDIDATCON_DMAEN) ? 1 : 0,
			(val & BIT_SDIDATCON_DSTART) ? 1 : 0,
			SHIFTR(val & MASK_SDIDATCON_BNUM, SHIFT_SDIDATCON_BNUM)
		);

	offset = REG_SDIDATCNT;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"BLOCKNUMCNT(%d)|BLOCKCNT(%d)",
			SHIFTR(val & MASK_SDIDATCNT_BSIZE, SHIFT_SDIDATCNT_BSIZE),
			SHIFTR(val & MASK_SDIDATCNT_BNUM, SHIFT_SDIDATCNT_BNUM)
		);

	offset = REG_SDIDATSTA;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"SSPDON(%1d)|RSTSSPD(%1d)|R1BTIMEO(%1d)|DSTART(%1d)|"
			"R1BFIN(%1d)|AUTOSTOP(%1d)|...|WAITR(%1d)|IRQ(%1d)|"
			"SNDCRC(%1d)|RCVCRC(%1d)|DTIMEO(%1d)|DFIN(%1d)"
			"BUSYFIN(%1d)|PROGERR(%1d)|TXON(%1d)|RXON(%1d)",
			(val & BIT_SDIDSTA_SSPDON) ? 1 : 0,
			(val & BIT_SDIDSTA_RSTSSPD) ? 1 : 0,
			(val & BIT_SDIDSTA_R1BTIMEOUT) ? 1 : 0,
			(val & BIT_SDIDSTA_DATASTART) ? 1 : 0,
			(val & BIT_SDIDSTA_R1BFIN) ? 1 : 0,
			(val & BIT_SDIDSTA_AUTOSTOP) ? 1 : 0,
			(val & BIT_SDIDSTA_WAITR) ? 1 : 0,
			(val & BIT_SDIDSTA_INTERRUPT) ? 1 : 0,
			(val & BIT_SDIDSTA_SNDCRC) ? 1 : 0,
			(val & BIT_SDIDSTA_RCVCRC) ? 1 : 0,
			(val & BIT_SDIDSTA_TIMEOUT) ? 1 : 0,
			(val & BIT_SDIDSTA_DATAFIN) ? 1 : 0,
			(val & BIT_SDIDSTA_BUSYFIN) ? 1 : 0,
			(val & BIT_SDIDSTA_PROGERR) ? 1 : 0,
			(val & BIT_SDIDSTA_TXON) ? 1 : 0,
			(val & BIT_SDIDSTA_RXON) ? 1 : 0
		);

	offset = REG_SDIFSTA;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"TXFULL(%1d)|TXEMPT(%1d)|...|RXFULL(%1d)|RXEMPT(%1d)|",
			(val & BIT_SDIFSTA_TXFULL) ? 1 : 0,
			(val & BIT_SDIFSTA_TXEMPT) ? 1 : 0,
			(val & BIT_SDIFSTA_RXFULL) ? 1 : 0,
			(val & BIT_SDIFSTA_RXEMPT) ? 1 : 0
		);

	offset = REG_SDIINTMSK;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"INTERRUPT MASK "
			"R1BFIN(%1d)|CMDRSPCRC(%1d)|CMDTIMEO(%1d)|CMDFIN(%1d)|"
			"EMMC(%1d)|PROGERR(%1d)|DSNDCRC(%1d)|DRCVCRC(%1d)|"
			"DTIMEO(%1d)|DFIN(%1d)",
			(val & BIT_SDIINT_R1BFIN) ? 1 : 0,
			(val & BIT_SDIINT_CMDRSPCRC) ? 1 : 0,
			(val & BIT_SDIINT_CMDTIMEOUT) ? 1 : 0,
			(val & BIT_SDIINT_CMDFIN) ? 1 : 0,
			(val & BIT_SDIINT_EMMC) ? 1 : 0,
			(val & BIT_SDIINT_PROGERR) ? 1 : 0,
			(val & BIT_SDIINT_DATASNDCRC) ? 1 : 0,
			(val & BIT_SDIINT_DATARCVCRC) ? 1 : 0,
			(val & BIT_SDIINT_DATATIMEOUT) ? 1 : 0,
			(val & BIT_SDIINT_DATAFIN) ? 1 : 0
		);

	offset = REG_SDIWRDAT;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"EMMC DATA"
		);

	offset = REG_SDISTAADD0;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD0"
		);

	offset = REG_SDISTAADD1;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD1"
		);

	offset = REG_SDISTAADD2;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD2"
		);

	offset = REG_SDISTAADD3;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD3"
		);

	offset = REG_SDISTAADD4;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD4"
		);

	offset = REG_SDISTAADD5;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD5"
		);

	offset = REG_SDISTAADD6;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD6"
		);

	offset = REG_SDISTAADD7;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"STAADD7"
		);

	offset = REG_SDIINTEN;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"INTERRUPT ENABLE "
			"R1BFIN(%1d)|CMDRSPCRC(%1d)|CMDTIMEO(%1d)|CMDFIN(%1d)|"
			"EMMC(%1d)|PROGERR(%1d)|DSNDCRC(%1d)|DRCVCRC(%1d)|"
			"DTIMEO(%1d)|DFIN(%1d)",
			(val & BIT_SDIINT_R1BFIN) ? 1 : 0,
			(val & BIT_SDIINT_CMDRSPCRC) ? 1 : 0,
			(val & BIT_SDIINT_CMDTIMEOUT) ? 1 : 0,
			(val & BIT_SDIINT_CMDFIN) ? 1 : 0,
			(val & BIT_SDIINT_EMMC) ? 1 : 0,
			(val & BIT_SDIINT_PROGERR) ? 1 : 0,
			(val & BIT_SDIINT_DATASNDCRC) ? 1 : 0,
			(val & BIT_SDIINT_DATARCVCRC) ? 1 : 0,
			(val & BIT_SDIINT_DATATIMEOUT) ? 1 : 0,
			(val & BIT_SDIINT_DATAFIN) ? 1 : 0
		);

	offset = REG_SDIDLLMVAL;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"DLLINIT(%1d)|DLLVAL(%d)", 
			(val & BIT_SDIDLLMVAL_INITDONE) ? 1 : 0,
			SHIFTR(val & MASK_SDIDLLMVAL_V, SHIFT_SDIDLLMVAL_V)
		);

	offset = REG_SDIDLLCON;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"RSYNCRD(%1d)|BYPASSRD(%1d)|RSYNCPAD(%1d)|BYPASSPAD(%1d)|"
			"MINIT(%1d)|MLOCK(%1d)|"
			"MSTART(%d)|MSTEP(%d)|MINTERVAL(%d)", 
			(val & BIT_SDIDLLCON_RSYNCRD) ? 1 : 0,
			(val & BIT_SDIDLLCON_BYPASSRD) ? 1 : 0,
			(val & BIT_SDIDLLCON_RSYNCPAD) ? 1 : 0,
			(val & BIT_SDIDLLCON_BYPASSPAD) ? 1 : 0,
			(val & BIT_SDIDLLCON_MINIT) ? 1 : 0,
			(val & BIT_SDIDLLCON_MLOCK) ? 1 : 0,
			SHIFTR(val & MASK_SDIDLLCON_MSTART, SHIFT_SDIDLLCON_MSTART),
			SHIFTR(val & MASK_SDIDLLCON_MSTEP, SHIFT_SDIDLLCON_MSTEP),
			SHIFTR(val & MASK_SDIDLLCON_MINTERVAL, SHIFT_SDIDLLCON_MINTERVAL)
		);

	offset = REG_SDICLKDELAY;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"DELAYRD(%d)|DELAYPAD(%d)",
			SHIFTR(val & MASK_SDICLKDELAY_RD, SHIFT_SDICLKDELAY_RD),
			SHIFTR(val & MASK_SDICLKDELAY_PAD, SHIFT_SDICLKDELAY_PAD)
		);

	offset = REG_SDISESEL;
	val = readl(regs + offset);
	__DUMP_PRINTF(offset, val,
			"BUS(%1d)|DATAMD(%1d)", 
			(val & BIT_SDISESEL_BUS) ? 1 : 0,
			(val & BIT_SDISESEL_DATA) ? 1 : 0
		);
#endif
}
EXPORT_SYMBOL(ls_mmc_dump_reg);

#if 0
// NON DMA read/write
static int ls_mmc_write_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	int sz = DIV_ROUND_UP(data->blocks * data->blocksize, 4);
	const void *buf = data->src;
	u32 val;

	printf("MCI: mci_write_data\n");

	while (sz--) {
		val = get_unaligned_le32(buf);
		writel(val, priv->regs + REG_SDIWRDAT);
		//val = readl(priv->regs + REG_SDIDATCON);
		//val |= BIT_SDIDATCON_DSTART;
		//writel(val, priv->regs + REG_SDIDATCON);
		buf += 4;
		udelay(10);
	}

	printf("MCI: mci_write_data: remaining %d\n", sz);
	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);
	ls_mmc_dump_reg();

	return 0;
}
static int ls_mmc_read_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	int sz = data->blocks * data->blocksize;
	void *buf = data->dest;
	u32 val, stat;

	printf("MCI: mci_read_data: %d\n", sz);

	do {
		do {
			val = readl(priv->regs + REG_SDIWRDAT);
			if (sz == 1)
				*(u8 *)buf = (u8)val;
			else if (sz == 2)
				put_unaligned_le16(val, buf);
			else if (sz >= 4)
				put_unaligned_le32(val, buf);
			buf += 4;
			sz -= 4;
			stat = readl(priv->regs + REG_SDIFSTA);
		} while (stat & BIT_SDIFSTA_RXFULL);

		do {
			stat = readl(priv->regs + REG_SDIDATSTA);
			printf("MCI DATA busy...\n");
//			ls_mmc_dump_reg();
			udelay(10);
		} while(stat & BIT_SDIDSTA_BUSYFIN);
	} while(sz > 0);
	printf("MCI: mci_read_data: remaining %d\n", sz);
	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);
	ls_mmc_dump_reg();

	return 0;
}
#else
static int ls_mmc_write_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	u32 val;

	ls_mmc_printf("MCI: mci_write_data, waiting finish\n");

	do {
		val = readl(priv->regs + REG_SDIINTMSK);
	} while(!(val & BIT_SDIINT_DATAFIN));

	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);
	ls_mmc_dump_reg();
	return 0;
}
static int ls_mmc_read_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	u32 val;

	ls_mmc_printf("MCI: mci_read_data, waiting finish...\n");

	do {
		val = readl(priv->regs + REG_SDIINTMSK);
	} while(!(val & BIT_SDIINT_DATAFIN));

	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);
	ls_mmc_dump_reg();
	return 0;
}
#endif

static int ls_mmc_setup_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	u32 val = 0;

	ls_mmc_printf("MCI: mci_setup_data: blocksize %d, blocks %d\n",
			data->blocksize, data->blocks);

	// setup data blocksize
	writel(SHIFTW(data->blocksize, SHIFT_SDIBSIZE_B) &
			MASK_SDIBSIZE_B,
			priv->regs + REG_SDIBSIZE);
	// setup data block count & control
	val = readl(priv->regs + REG_SDIDATCON);
	val = (val & (~MASK_SDIDATCON_BNUM)) |
		(SHIFTW(data->blocks, SHIFT_SDIDATCON_BNUM) &
		 	MASK_SDIDATCON_BNUM);

	val |= BIT_SDIDATCON_WIDEMD8B | BIT_SDIDATCON_DSTART | BIT_SDIDATCON_DMAEN;

	writel(val, priv->regs + REG_SDIDATCON);

	return 0;
}

static int ls_mmc_prepare_dma(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	ls_mmc_printf("MCI: ls_mci_prepare_dma\n");

	u64 wdma_base     = (u64)(priv->regs + 0x400);
	u64 rdma_base     = (u64)(priv->regs + 0x800);
	u64 fifo_base     = (u64)(priv->regs + 0x40);
	u64 data_phy_addr = (u64)(data->dest);
	u32 blk_size      = data->blocksize;

	/* switch to phy addr */
	u32	dma_desc_addr = 0x80000;

	if (data->flags & MMC_DATA_WRITE)
		data_phy_addr = (u64)(data->src);

	/* flush cache!!!!!!!!!!!!!!!!!!!!!!*/
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x0))   = 0x0;                               //next dma desc addr is invalid
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x4))   = (u32)data_phy_addr;                //addr in mem
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x8))   = fifo_base;                         //
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0xc))   = (data->blocks * blk_size) / 4;     //data length
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x24))  = 0x90000000 | (data_phy_addr >> 32);  
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x10))  = 0x0;                               //dma_step_length 
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x14))  = 0x1;                               //dma_step_times
	    											    //
	if(data->flags == MMC_DATA_READ){
		//read
 		*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x18))  = 0x0;
		//dma desc addr
		*(volatile u32 *)rdma_base  = dma_desc_addr + 0x8;
	}
	else{
		// write
		*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x18))  = 0x1000;
		//dma desc addr
		*(volatile u32 *)wdma_base  = dma_desc_addr + 0x8;
	}
	return 0;
}

static int ls_mmc_send_cmd(struct ls_mmc_priv *priv,
			   struct mmc_cmd *cmd, struct mmc_data *data)
{
	int ret;

	u32 cmdidx = cmd->cmdidx;
	u32 val = 0;

	ls_mmc_printf("MCI: mci_send_command: cmd %d, resp_type %d, arg 0x%x\n",
			cmdidx, cmd->resp_type, cmd->cmdarg);

	// clear all interrupt
	writel(SDIINT_ALL, priv->regs + REG_SDIINTMSK);

	if (data) {
		ls_mmc_setup_data(priv, data);
		ls_mmc_prepare_dma(priv, data);
	}

	// setup command arg
	writel(cmd->cmdarg, priv->regs + REG_SDICMDARG);
	// setup command index & send
	/* Emmc cmd6 do not need data transfer */
	val = (SHIFTW(cmdidx, SHIFT_SDICMDCON_CMDIDX) & MASK_SDICMDCON_CMDIDX) |
		BIT_SDICMDCON_CMDSTART | BIT_SDICMDCON_SENDERHOST;
	if (cmdidx == MMC_CMD_SWITCH && data)
		val |= SDICMDCON_CMD6DATA;
	if (cmd->resp_type & MMC_RSP_PRESENT)
		val |= BIT_SDICMDCON_WAITRSP;
	if (cmd->resp_type & MMC_RSP_136)
		val |= BIT_SDICMDCON_LONGRSP;
	writel(val, priv->regs + REG_SDICMDCON);

	// wait command finish
	mdelay(1);

	// get command response
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = readl(priv->regs + REG_SDIRSP0);
		cmd->response[1] = readl(priv->regs + REG_SDIRSP1);
		cmd->response[2] = readl(priv->regs + REG_SDIRSP2);
		cmd->response[3] = readl(priv->regs + REG_SDIRSP3);
	}

	ls_mmc_dump_reg();

	// error handle
	val = readl(priv->regs + REG_SDIINTMSK);
	if (val & BIT_SDIINT_CMDTIMEOUT)
		return -ETIMEDOUT;

	if (data) {
		// 无 DMA 时 send/rcv data
		// 有 DMA 时只将 DATA 寄存器清空
		if (data->flags & MMC_DATA_WRITE)
			ret = ls_mmc_write_data(priv, data);
		else if (data->flags & MMC_DATA_READ)
			ret = ls_mmc_read_data(priv, data);

		if (ret)
			return ret;
	}

	return 0;
}

static int ls_mmc_set_ios(struct ls_mmc_priv *priv, u32 clock)
{
	u32 ref_clock = priv->ref_clock;
	u32 val = readl(priv->regs + REG_SDIPRE);
	u8 mci_psc;
	u32 approximate_clock;

	// Calculate clock
	for (mci_psc = 1; mci_psc < 255; mci_psc++) {
		approximate_clock = ref_clock / mci_psc;

		if (approximate_clock <= clock)
			break;
	}

	val = (val & (~MASK_SDIPRE_SCALE)) | 
		(SHIFTW(mci_psc, SHIFT_SDIPRE_SCALE) & MASK_SDIPRE_SCALE);

	writel(val, priv->regs + REG_SDIPRE);

	printf("MCI: ios clock %u, refclock %u, approximate %u, [0x%x]\n",
			clock, ref_clock, approximate_clock, val);

	mdelay(1);

	return 0;
}

static int ls_mmc_core_init(struct ls_mmc_priv *priv)
{
	u32 val;

	/* Reset */
	writel(BIT_SDICON_RESET,
			priv->regs + REG_SDICON);
	val = readl(priv->regs + REG_SDICON);
	mdelay(100);

	// Disable ALL IRQs
	//writel(0x00, priv->regs + REG_SDIINTEN);
	// Enable ALL IRQs
	writel(SDIINT_ALL, priv->regs + REG_SDIINTEN);

	// Bus EMMC SDR
	//writel(SDISESEL_BUS_EMMC | SDISESEL_DATA_SDR,
	//		priv->regs + REG_SDISESEL);

	// Set Data timeouts MAX
	writel(MASK_SDIDTIMER_T,
			priv->regs + REG_SDIDTIMER);

	// Data Control 8 Bits
	//writel(BIT_SDIDATCON_WIDEMD8B,
	//		priv->regs + REG_SDIDATCON);

	// NEED DLL Master INIT?
	// DLL rd-resync rd-bypass
	//writel(BIT_SDIDLLCON_RSYNCRD | BIT_SDIDLLCON_BYPASSRD,
	//		priv->regs + REG_SDIDLLCON);

	// Maximum DLL delay
	//writel(MASK_SDICLKDELAY_RD | MASK_SDICLKDELAY_PAD,
	//		priv->regs + REG_SDICLKDELAY);

	// Clock Rev
	writel(BIT_SDIPRE_REVCLK,
			priv->regs + REG_SDIPRE);

	// Enable Clock & Unknown BIT
	writel(BIT_SDICON_CLKEN | BIT_SDICON_UNKWN,
			priv->regs + REG_SDICON);

	mdelay(1);

	ls_mmc_dump_reg();

	return 0;
}

static int dm_ls_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			      struct mmc_data *data)
{
	struct ls_mmc_priv *priv = dev_get_priv(dev);
	return ls_mmc_send_cmd(priv, cmd, data);
}

static int dm_ls_mmc_set_ios(struct udevice *dev)
{
	struct ls_mmc_priv *priv = dev_get_priv(dev);
	struct mmc *mmc = mmc_get_mmc_dev(dev);

	return ls_mmc_set_ios(priv, mmc->clock);
};

static const struct dm_mmc_ops dmops_ls_mmc = {
	.send_cmd	= dm_ls_mmc_send_cmd,
	.set_ios	= dm_ls_mmc_set_ios,
};

static int drv_ls_mmc_of_to_plat(struct udevice *dev)
{
	struct ls_mmc_priv *priv = dev_get_priv(dev);
	struct ls_mmc_plat *plat = dev_get_plat(dev);
	struct mmc_config *cfg;
	struct mmc *mmc;
	struct blk_desc *bdesc;

	priv->regs = map_physmem(dev_read_addr(dev), 0x8000, MAP_NOCACHE);
	//priv->regs = PHYS_TO_UNCACHED(0x14210000);
	priv->ref_clock = 200000000;

	cfg = &plat->cfg;
	cfg->name = "loongson_emmc";
	cfg->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS;
	cfg->f_min = priv->ref_clock / 256;
	cfg->f_max = priv->ref_clock;
	cfg->voltages = MMC_VDD_32_33|MMC_VDD_33_34;
	cfg->b_max = MMC_MAX_BLOCK_LEN;

	// MMC 手动初始化参数
	mmc = &plat->mmc;
	mmc->read_bl_len = MMC_MAX_BLOCK_LEN;
	mmc->write_bl_len = MMC_MAX_BLOCK_LEN;
	mmc->capacity_user = SZ_8G;
	mmc->capacity_user *= mmc->read_bl_len;
	mmc->capacity_boot = 0;
	mmc->capacity_rpmb = 0;
	for (int i = 0; i < 4; i++)
		mmc->capacity_gp[i] = 0;
	mmc->capacity = SZ_8G;
	mmc->rca = 1;
	mmc->high_capacity = 1;

	bdesc = mmc_get_blk_desc(mmc);
	bdesc->lun = 0;
	bdesc->hwpart = 0;
	bdesc->type = 0;
	bdesc->blksz = mmc->read_bl_len;
	bdesc->log2blksz = LOG2(bdesc->blksz);
	bdesc->lba = lldiv(mmc->capacity, mmc->read_bl_len);

	return 0;
}

static int drv_ls_mmc_bind(struct udevice *dev)
{
	struct ls_mmc_plat *plat = dev_get_plat(dev);

	return mmc_bind(dev, &plat->mmc, &plat->cfg);
}

static int drv_ls_mmc_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct ls_mmc_priv *priv = dev_get_priv(dev);
	struct ls_mmc_plat *plat = dev_get_plat(dev);
	struct mmc_config *cfg = &plat->cfg;
	// pmon init cmds
	struct mmc_cmd init_cmds[] = {
		{.cmdidx = 0x00, .resp_type = 0xc0, .cmdarg = 0x00},
		{.cmdidx = 0x01, .resp_type = 0xe1, .cmdarg = 0x00},
		// 重复 CMD1 ARG-0，否则首次上电后续命令超时
		{.cmdidx = 0x01, .resp_type = 0xe1, .cmdarg = 0x00},
		{.cmdidx = 0x00, .resp_type = 0xc0, .cmdarg = 0x00},
		{.cmdidx = 0x01, .resp_type = 0xe1, .cmdarg = 0x40200000},
		// 重复 CMD1 ARG-0x40200000，否则后续命令超时
		{.cmdidx = 0x01, .resp_type = 0xe1, .cmdarg = 0x40200000},
		{.cmdidx = 0x01, .resp_type = 0xe1, .cmdarg = 0x40200000},
		{.cmdidx = 0x02, .resp_type = 0x07, .cmdarg = 0x00},
		{.cmdidx = 0x03, .resp_type = 0x15, .cmdarg = 0x10000},
		{.cmdidx = 0x09, .resp_type = 0x07, .cmdarg = 0x10000},
		{.cmdidx = 0x07, .resp_type = 0x15, .cmdarg = 0x10000},
		{.cmdidx = 0x06, .resp_type = 0x49d, .cmdarg = 0x3b70201},
	};

	plat->mmc.priv = priv;
	upriv->mmc = &plat->mmc;

	ls_mmc_core_init(priv);

	// 先以低频运行 init_cmd，init_cmd 在频率大于 1MHZ 会超时
	ls_mmc_set_ios(priv, cfg->f_min);

	for (int i = 0; i < ARRAY_SIZE(init_cmds); i++)
	{
		if (ls_mmc_send_cmd(priv, init_cmds + i, NULL))
			return -ETIMEDOUT;
		// 适当延时，否则首次上电初始化命令会不响应
		mdelay(20);
	}

	ls_mmc_set_ios(priv, 50000000);

	// 不进行 uboot mmc init，因为部分命令不响应
	upriv->mmc->has_init = 1;

	return 0;
}

static const struct udevice_id drvids_ls_mmc[] = {
	{ .compatible = "loongson,ls-emmc" },
	{ }
};

U_BOOT_DRIVER(ls_emmc_drv) = {
	.name = "ls_emmc",
	.id = UCLASS_MMC,
	.of_match = drvids_ls_mmc,
	.of_to_plat = drv_ls_mmc_of_to_plat,
	.bind = drv_ls_mmc_bind,
	.probe = drv_ls_mmc_probe,
	.priv_auto = sizeof(struct ls_mmc_priv),
	.plat_auto = sizeof(struct ls_mmc_plat),
	.ops = &dmops_ls_mmc,
};
