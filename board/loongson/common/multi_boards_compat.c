#include <common.h>
#include <image.h>
#include <dm.h>
#include <dm/root.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <handoff.h>
#include "loongson_board_info.h"
#include "bdinfo/bdinfo.h"

DECLARE_GLOBAL_DATA_PTR;

enum board_type {
	BOARD_TYPE_UNKNOWN = 0,
#if defined(CONFIG_SOC_LS2K500)
	BOARD_TYPE_LS2K500_EVB,
	BOARD_TYPE_LS2K500_HL_MB,
	BOARD_TYPE_LS2K500_MINI_DP,
	BOARD_TYPE_LS2K500_MODI_HCT,
#elif defined(CONFIG_SOC_LS2K1000)
	BOARD_TYPE_LS2K1000_JL_MB,
	BOARD_TYPE_LS2K1000_DP,
	BOARD_TYPE_LS2K1000_GBKPDM0_V10,
#endif
	BOARD_TYPE_MAX,
};

struct ls_board {
	enum board_type bdtype;
	char *show_name;
	char *dtb_name;		// without the suffix .dtb
	char *boardid;
};

static struct ls_board available_boards[] = {
#if defined(CONFIG_SOC_LS2K500)
	{BOARD_TYPE_LS2K500_EVB, LS2K500_EVB_DESC, "ls2k500_evb", LS2K500_EVB_BOARD_NAME},
	{BOARD_TYPE_LS2K500_HL_MB, LS2K500_HL_DESC, "ls2k500_hl_mb", LS2K500_HL_BOARD_NAME},
	{BOARD_TYPE_LS2K500_MINI_DP, LS2K500_MINI_DP_DESC, "ls2k500_mini_dp", LS2K500_MINI_DP_BOARD_NAME},
	{BOARD_TYPE_LS2K500_MODI_HCT, LS2K500_MODI_HCT_DESC, "ls2k500_modi_hct", LS2K500_MODI_HCT_BOARD_NAME},
#elif defined(CONFIG_SOC_LS2K1000)
	{BOARD_TYPE_LS2K1000_JL_MB, LS2K1000_JL_DESC, "ls2k1000_jl_mb", LS2K1000_JL_BOARD_NAME},
	{BOARD_TYPE_LS2K1000_DP, LS2K1000_DP_DESC, "ls2k1000_dp", LS2K1000_DP_BOARD_NAME},
	{BOARD_TYPE_LS2K1000_GBKPDM0_V10, LS2K1000_GBKPDM0_V10_DESC, "ls2k1000_gbkpdm0_v10", LS2K1000_GBKPDM0_V10_BOARD_NAME},
#endif
};

#if !defined(CONFIG_SPL) || defined(CONFIG_SPL_BUILD)

#ifdef CONFIG_SOC_LS2K500
static struct ls_board board_default = {
	.bdtype = BOARD_TYPE_UNKNOWN,
	.show_name = "LS2K500-Default",
	.dtb_name = "ls2k500_default",
	.boardid = NULL,
};
#elif defined(CONFIG_SOC_LS2K1000)
static struct ls_board board_default = {
	.bdtype = BOARD_TYPE_UNKNOWN,
	.show_name = "LS2K1000-Default",
	.dtb_name = "ls2k1000_default",
	.boardid = NULL,
};
#endif

static char* user_select_board_id(void)
{
	char *boardid = "unknown";
	int i, cnt, choice, id = 0;
	char input[8] = {0};

	puts("\nCan not get the board type from EEPROM/SPI-flash.\n");
	puts("You can select the right board type from below\n\n");
	puts("Available boards:\n");
	cnt = ARRAY_SIZE(available_boards);
	for (i = 0; i < cnt; ++i) {
		printf("\t[%d] %s\n", i + 1, available_boards[i].show_name);
	}
	puts("Please input the chosen: ");

get_user_input:
	while (!tstc()) {
		choice = getchar();
		if (choice == '\n' || choice == '\r') {	// press 'Enter' key
			puts("\n");
			break;
		} else if (choice >= '0' && choice <= '9') {
			input[id++] = choice & 0xff;
			putc(choice);
		} else {
			// unrecognized input
			memset(input, 0, sizeof(input));
			id = 0;
		}
		udelay(10000);
	}

	if (input[0]) {
		id = simple_strtoul(input, NULL, 10);
		id--;

		if (id >= cnt) {
			printf("the input is exceeded, please reselect: ");
			goto get_user_input;
		} else {
			boardid = available_boards[id].boardid;
			bdinfo_set(BDI_ID_NAME, boardid);
#ifndef CONFIG_SPL
			if (bdinfo_save()) {
				printf("save bdinfo failed\n");
			}
#endif
		}
	}

	return boardid;
}

static void do_board_detect(void)
{
	char *boardid;
	int i, cnt;

	gd->board_type = BOARD_TYPE_UNKNOWN;

	boardid = bdinfo_get(BDI_ID_NAME);

	if (!boardid || !strcmp(boardid, "unknown")) {
		// can not get board id, we get a chance to select the board id by serial.
		boardid = user_select_board_id();
		printf("user select boardid: %s\n", boardid);
	}

	cnt = ARRAY_SIZE(available_boards);
	for (i = 0; i < cnt; ++i) {
		if (!strcmp(boardid, available_boards[i].boardid)) {
			gd->board_type = available_boards[i].bdtype;
			break;
		}
	}
}

static int do_dtb_select(void)
{
	int res;
	int rescan = 0;

	do_board_detect();

	res = fdtdec_resetup(&rescan);
	if (!res && rescan) {
		dm_uninit();
		dm_init_and_scan(true);
	}

	return 0;
}

int board_fit_config_name_match(const char *name)
{
	struct ls_board *bd = NULL;
	int cnt, i;

	if (gd->board_type == BOARD_TYPE_UNKNOWN
		&& !strncmp(name, board_default.dtb_name, strlen(name)))
		return 0;

	cnt = ARRAY_SIZE(available_boards);
	for (i = 0; i < cnt; ++i) {
		if (gd->board_type == available_boards[i].bdtype) {
			bd = &available_boards[i];
			break;
		}
	}

	if (!bd || !bd->dtb_name || bd->dtb_name[0] == '\0')
		goto mismatch;

	if (!strcmp(name, bd->dtb_name)) {
		debug("Found the matching dtb: %s.dtb\n", name);
		return 0;
	}

mismatch:
	debug("Mismatch dtb: %s.dtb!!\n", name);
	return -ENOENT;
}
#endif

#ifdef CONFIG_DTB_RESELECT
int embedded_dtb_select(void)
{
	return do_dtb_select();
}
#endif

#ifdef CONFIG_SPL_BUILD
int spl_dtb_select(void)
{
	int ret;

	ret = bdinfo_init();
	if (ret)
		return ret;

	return do_dtb_select();
}
#endif

#if defined(CONFIG_SPL) && !defined(CONFIG_SPL_BUILD)
int multi_boards_check_store(void)
{
	struct spl_handoff *ho = gd->spl_handoff;
	char *boardid = NULL, *bdiname;
	int i, cnt, ret;

	if (!ho || ho->arch.board_type >= BOARD_TYPE_MAX) {
		gd->board_type = BOARD_TYPE_UNKNOWN;
		return -EINVAL;
	}

	gd->board_type = ho->arch.board_type;

	cnt = ARRAY_SIZE(available_boards);
	for (i = 0; i < cnt; ++i) {
		if (gd->board_type == available_boards[i].bdtype) {
			boardid = available_boards[i].boardid;
			break;
		}
	}

	if (boardid) {
		bdiname = bdinfo_get(BDI_ID_NAME);
		if (!bdiname || strcmp(bdiname, boardid)) {
			debug("saving bdinfo...");
			bdinfo_set(BDI_ID_NAME, boardid);
			ret = bdinfo_save();
			if (ret) {
				debug("failed\n");
				return ret;
			}
			debug("OK\n");
		}
	}

	return 0;
}
#endif
