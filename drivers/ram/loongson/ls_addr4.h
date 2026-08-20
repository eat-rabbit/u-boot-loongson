#ifndef __LOONGSON_DDR_H__
#define __LOONGSON_DDR_H__
#include <dm/device.h>
//#include <asm-generic/types.h>
#include <config.h>
#include <ram.h>
#include "mem_ctrl.h"

// #pragma   pack(4)
struct mc_setting {
	struct udevice	*dev;
	struct ram_info	info;
    struct ddr_ctrl *mm_ctrl_info;

	u8	cs_map;
	u8	memsize;
	u8	slice_num;

	u8	sdram_type;
	u8	sdram_width;
	u8	sdram_banks;
	u8	sdram_bank_groups;
	u8	sdram_rows;
	u8	sdram_cols;

	u8	dimm_type;
	u8	data_width;

	u8	ecc_en;
	u8	addr_mirror;
	u8	early_print;
	bool reset_revert;
	u8  ddr_param_store;

	u8	clk_latency;
	u8	rd_latency;
	u8	wr_latency;
	u8	mc_dqs_odt;
	u8	cmd_timing_mode;
};

// sdram width
#define MC_SDRAM_WIDTH_X4	(0)
#define MC_SDRAM_WIDTH_X8	(1)
#define MC_SDRAM_WIDTH_X16	(2)

// cs map
#define MC_USE_CS_0		(1 << 0)
#define MC_USE_CS_1		(1 << 1)
#define MC_USE_CS_2		(1 << 2)
#define MC_USE_CS_3		(1 << 3)
#define MC_USE_CS_ALL		(MC_USE_CS_0 | MC_USE_CS_1 | MC_USE_CS_2 | MC_USE_CS_3)

// addr mirror
#define MC_ADDR_MIRROR_NO	(0)
#define MC_ADDR_MIRROR_YES	(1)

// sdram type
#define MC_SDRAM_TYPE_NODIMM	(0)
#define MC_SDRAM_TYPE_DDR3	(0xb)
#define MC_SDRAM_TYPE_DDR4	(0xc)

// sdram rows
#define MC_SDRAM_ROW(n)		(18 - (n))

// sdram cols
#define MC_SDRAM_COL(n)		(12 - (n))

// sdram banks
#define MC_SDRAM_BANK_4		(0)
#define MC_SDRAM_BANK_8		(1)

// sdram of bank groups
#define MC_SDRAM_BANK_GROUPS_0		(0)
#define MC_SDRAM_BANK_GROUPS_2		(1)
#define MC_SDRAM_BANK_GROUPS_4		(2)

// dimm type
#define MC_DIMM_TYPE_UDIMM	(0)
#define MC_DIMM_TYPE_RDIMM	(1)
#define MC_DIMM_TYPE_SODIMM	(2)

// ecc
#define MC_ECC_EN_NO		(0)
#define MC_ECC_EN_YES		(1)

// early printf
#define MC_EARLY_PRINT_NO	(0)
#define MC_EARLY_PRINT_YES	(1)

// ddr param store
#define MC_DDR_PARAM_STORE_NO	(0)
#define MC_DDR_PARAM_STORE_YES	(1)

// cmd timing mode
#define MC_CMD_TIMING_MODE_1T	0
#define MC_CMD_TIMING_MODE_2T	1
#define MC_CMD_TIMING_MODE_3T	2

#endif
