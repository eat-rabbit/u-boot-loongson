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

#define	DMA_CMD_INTMSK		BIT(0)
#define	DMA_CMD_WRITE		BIT(12)
#define	DMA_OREDER_ASK		BIT(2)
#define	DMA_OREDER_START	BIT(3)
#define	DMA_OREDER_STOP		BIT(4)
#define	DMA_ALIGNED		32
#define	DMA_ALIGNED_MSK		(~(DMA_ALIGNED - 1))


struct ls_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct ls_mmc_priv {
	void __iomem *regs;
	u32 ref_clock;
	int buswidth;
};

struct dma_desc {
	unsigned int order_addr_low;
	unsigned int saddr_low;
	unsigned int daddr;
	unsigned int length;
	unsigned int step_length;
	unsigned int step_times;
	unsigned int cmd;
	unsigned int order_addr_high;
	unsigned int saddr_high;
};

static int ls_mmc_int_idle(struct ls_mmc_priv* priv, u32 mask, u32 correct)
{
	u32 val;
	do {
		val = readl(priv->regs + REG_SDIINTMSK);
	} while(!(val & mask));

	writel(mask, priv->regs + REG_SDIINTMSK);

	if (val & correct)
		return 0;

	ls_mmc_printf("LS-MMC INT IDLE(0x%x) ERR: 0x%x\n", mask, val);

	if (val & BIT_SDIINT_CMDTIMEOUT ||
		val & BIT_SDIINT_DATATIMEOUT)
		return -ETIMEDOUT;

	return -1;
}

#if 1
// NON DMA read/write
static int ls_mmc_write_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	int sz = data->blocks * data->blocksize;
	const void *buf = data->src;
	u32 stat;
	u32 val;

	ls_mmc_printf("MCI: mci_write_data %d\n", sz);

	while (sz > 0) {
		if (sz == 1)
		{
			val = *((u8*)buf);
			buf += 1;
			sz -= 1;
		}
		else if (sz == 2)
		{
			val = get_unaligned_le16(buf);
			buf += 2;
			sz -= 2;
		}
		else if (sz == 3)
		{
			val = get_unaligned_le24(buf);
			buf += 3;
			sz -= 3;
		}
		else
		{
			val = get_unaligned_le32(buf);
			buf += 4;
			sz -= 4;
		}
		writel(val, priv->regs + REG_SDIWRDAT);

		do {
			stat = readl(priv->regs + REG_SDIDATSTA);
			ls_mmc_printf("MCI DATA busy... 0x%x\n", stat);
			udelay(50);
		} while(stat & BIT_SDIDSTA_BUSYFIN);
	}

	ls_mmc_printf("MCI: mci_write_data: remaining %d\n", sz);
	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);

	return 0;
}

static int ls_mmc_read_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	int sz = data->blocks * data->blocksize;
	void *buf = data->dest;
	u32 val, stat;

	ls_mmc_printf("MCI: mci_read_data: %d\n", sz);

	do {
		do {
			val = readl(priv->regs + REG_SDIWRDAT);
			if (sz == 1)
			{
				*(u8 *)buf = (u8)val;
				buf += 1;
				sz -= 1;
			}
			else if (sz == 2)
			{
				put_unaligned_le16(val, buf);
				buf += 2;
				sz -= 2;
			}
			else if (sz == 3)
			{
				put_unaligned_le24(val, buf);
				buf += 3;
				sz -= 3;
			}
			else
			{
				put_unaligned_le32(val, buf);
				buf += 4;
				sz -= 4;
			}
			stat = readl(priv->regs + REG_SDIFSTA);
		} while (stat & BIT_SDIFSTA_RXFULL);

		do {
			stat = readl(priv->regs + REG_SDIDATSTA);
			ls_mmc_printf("MCI DATA busy... 0x%x\n", stat);
			udelay(10);
		} while(stat & BIT_SDIDSTA_BUSYFIN);
	} while(sz > 0);

	ls_mmc_printf("MCI: mci_read_data: remaining %d\n", sz);
	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);

	return 0;
}
#else
static int ls_mmc_write_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	int ret;

	ls_mmc_printf("MCI: mci_write_data, waiting finish\n");

	ret = ls_mmc_int_idle(priv,
			(BIT_SDIINT_PROGERR | BIT_SDIINT_DATASNDCRC
			| BIT_SDIINT_DATARCVCRC | BIT_SDIINT_DATATIMEOUT
			| BIT_SDIINT_DATAFIN),
			BIT_SDIINT_DATAFIN);

	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);

	return ret;
}
static int ls_mmc_read_data(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	int ret;

	ls_mmc_printf("MCI: mci_read_data, waiting finish...\n");

	ret = ls_mmc_int_idle(priv,
			(BIT_SDIINT_PROGERR | BIT_SDIINT_DATASNDCRC
			| BIT_SDIINT_DATARCVCRC | BIT_SDIINT_DATATIMEOUT
			| BIT_SDIINT_DATAFIN),
			BIT_SDIINT_DATAFIN);

	writel(0, priv->regs + REG_SDIBSIZE);
	writel(0, priv->regs + REG_SDIDATCON);

	return ret;
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
	val = SHIFTW(data->blocks, SHIFT_SDIDATCON_BNUM) & MASK_SDIDATCON_BNUM;

	if (priv->buswidth == 4)
		val |= BIT_SDIDATCON_WIDEMD;
	val |= BIT_SDIDATCON_DSTART;

	writel(val, priv->regs + REG_SDIDATCON);

	return 0;
}
#if 0
static int ls_mmc_prepare_dma(struct ls_mmc_priv *priv, struct mmc_data *data)
{
	ls_mmc_printf("MCI: ls_mci_prepare_dma\n");
#if 0
	struct dma_desc* desc = (struct dma_desc*)PHYS_TO_UNCACHED(0x10000);
	int data_size = data->blocksize * data->blocks;
	unsigned int data_phy_addr;
	void *dma_order_addr;

	if (data->flags == MMC_DATA_READ) {
		data_phy_addr = (unsigned int)data->dest;
	} else {
		data_phy_addr = (unsigned int)data->src;
	}

//	if (desc == NULL || !((unsigned long long)desc & UNCACHED_MEMORY_ADDR) || !host->wdma_order_addr || !host->rdma_order_addr) {
//		printf("Invalid DMA address.\n");
//		return -EINVAL;
//	}

	desc->order_addr_low	= 0x0;
	desc->saddr_low		= data_phy_addr;
	desc->daddr		= priv->regs + REG_SDIWRDAT;
	desc->length		= data_size / 4;
	desc->step_length	= 0x1;
	desc->step_times	= 0x1;

	pr_debug("data_phy_addr =  0x%x,data->flags=%d\n",data_phy_addr,data->flags);
	pr_debug("desc->order_addr_low = 0x%x\n",desc->order_addr_low);
	pr_debug("desc->saddr_low = 0x%x\n",desc->saddr_low);
	pr_debug("desc->daddr = 0x%x\n",desc->daddr);
	pr_debug("desc->length = 0x%x\n",desc->length);

	if (data->flags == MMC_DATA_READ) {
		desc->cmd	= DMA_CMD_INTMSK;
		dma_order_addr	= priv->regs + 0x800;
	} else {
		desc->cmd	= DMA_CMD_INTMSK | DMA_CMD_WRITE;
		dma_order_addr	= priv->regs + 0x400;
	}

	writel(((0x10000) << 1) | 1, dma_order_addr);
	//outl(dma_order_addr, (unsigned int)desc | DMA_OREDER_START);

	return 0;
#else
	u64 wdma_base     = (u64)(priv->regs + 0x400);
	u64 rdma_base     = (u64)(priv->regs + 0x800);
	u64 fifo_base     = (u64)(priv->regs + 0x40);
	u64 data_phy_addr = (u64)(data->dest);
	u32 blk_size      = data->blocksize;

	/* switch to phy addr */
	u32	dma_desc_addr = 0x10000;

	if (data->flags & MMC_DATA_WRITE)
		data_phy_addr = (u64)(data->src);

	/* flush cache!!!!!!!!!!!!!!!!!!!!!!*/
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x0))   = 0x0;                               //next dma desc addr is invalid
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x4))   = (u32)data_phy_addr;                //addr in mem
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x8))   = fifo_base;                         //
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0xc))   = (data->blocks * blk_size) / 4;     //data length
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x24))  = 0x90000000 | (data_phy_addr >> 32);  
	*(volatile u32 *)(PHYS_TO_UNCACHED(dma_desc_addr + 0x10))  = 0x1;                               //dma_step_length 
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
#endif
}
#endif
static int ls_mmc_send_cmd(struct ls_mmc_priv *priv,
			   struct mmc_cmd *cmd, struct mmc_data *data)
{
	int ret;

	u32 cmdidx = cmd->cmdidx;
	u32 val = 0;

	ls_mmc_printf("MCI: mci_send_command: cmd %d, resp_type %d, arg 0x%x\n",
			cmdidx, cmd->resp_type, cmd->cmdarg);
	mdelay(1);

	// clear all interrupt
	writel(SDIINT_ALL, priv->regs + REG_SDIINTMSK);

	if (data) {
		ls_mmc_setup_data(priv, data);
		//ls_mmc_prepare_dma(priv, data);
	}

	// setup command arg
	writel(cmd->cmdarg, priv->regs + REG_SDICMDARG);
	// setup command index & send
	/* Emmc cmd6 do not need data transfer */
	val = (SHIFTW(cmdidx, SHIFT_SDICMDCON_CMDIDX) & MASK_SDICMDCON_CMDIDX) |
		BIT_SDICMDCON_CMDSTART | BIT_SDICMDCON_SENDERHOST;
	//if (cmdidx == MMC_CMD_SWITCH && data)
	//	val |= SDICMDCON_CMD6DATA;
	// Do Not AUTO STOP
	//if (cmdidx & MMC_CMD_READ_MULTIPLE_BLOCK ||
	//	cmdidx & MMC_CMD_WRITE_MULTIPLE_BLOCK)
	//	val |= BIT_SDICMDCON_AUTOSTOPEN;
	if (cmd->resp_type & MMC_RSP_PRESENT)
		val |= BIT_SDICMDCON_WAITRSP;
	if (cmd->resp_type & MMC_RSP_CRC)
		val |= BIT_SDICMDCON_CHECKCRC;
	if (cmd->resp_type & MMC_RSP_136)
		val |= BIT_SDICMDCON_LONGRSP;

	writel(val, priv->regs + REG_SDICMDCON);

	mdelay(1);
	// wait command finish
	ret = ls_mmc_int_idle(priv,
			BIT_SDIINT_CMDRSPCRC | BIT_SDIINT_CMDTIMEOUT|
			BIT_SDIINT_CMDFIN,
			BIT_SDIINT_CMDFIN);
	if (ret)
		return ret;

	// get command response
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = readl(priv->regs + REG_SDIRSP0);
		if (val & BIT_SDICMDCON_LONGRSP)
		{
			cmd->response[1] = readl(priv->regs + REG_SDIRSP1);
			cmd->response[2] = readl(priv->regs + REG_SDIRSP2);
			cmd->response[3] = readl(priv->regs + REG_SDIRSP3);
		}
	}

	mdelay(1);

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
	writel(SDISESEL_BUS_SDIO | SDISESEL_DATA_SDR,
			priv->regs + REG_SDISESEL);

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

	return 0;
}

static int dm_ls_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			      struct mmc_data *data)
{
	struct ls_mmc_priv *priv = dev_get_priv(dev);
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	priv->buswidth = mmc->bus_width;
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

	priv->regs = map_physmem(dev_read_addr(dev), 0x8000, MAP_NOCACHE);
	priv->ref_clock = 200000000;

	cfg = &plat->cfg;
	cfg->name = "loongson_sdcard";
	cfg->host_caps = MMC_MODE_HS | MMC_MODE_4BIT | MMC_MODE_1BIT;
	cfg->f_min = priv->ref_clock / 256;
	cfg->f_max = priv->ref_clock / 4;
	cfg->voltages = MMC_VDD_33_34;
	cfg->b_max = MMC_MAX_BLOCK_LEN;

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

	plat->mmc.priv = priv;
	upriv->mmc = &plat->mmc;

	ls_mmc_core_init(priv);

	// 先以低频运行
	ls_mmc_set_ios(priv, cfg->f_min);

	return 0;
}

static const struct udevice_id drvids_ls_mmc[] = {
	{ .compatible = "loongson,ls-sdcard" },
	{ }
};

U_BOOT_DRIVER(ls_sdcard_drv) = {
	.name = "ls_sdcard",
	.id = UCLASS_MMC,
	.of_match = drvids_ls_mmc,
	.of_to_plat = drv_ls_mmc_of_to_plat,
	.bind = drv_ls_mmc_bind,
	.probe = drv_ls_mmc_probe,
	.priv_auto = sizeof(struct ls_mmc_priv),
	.plat_auto = sizeof(struct ls_mmc_plat),
	.ops = &dmops_ls_mmc,
};
