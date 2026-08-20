#include <stdio.h>
#include <string.h>
#include "loongson_board_info.h"

#if defined(CONFIG_SOC_LS2K500)
static char* board_desc_set[] = {LS2K500_EVB_DESC, LS2K500_HL_DESC, LS2K500_MINI_DP_DESC, LS2K500_MODI_HCT_DESC, NULL};
static char* board_name_set[] = {LS2K500_EVB_BOARD_NAME, LS2K500_HL_BOARD_NAME, LS2K500_MINI_DP_BOARD_NAME, LS2K500_MODI_HCT_BOARD_NAME, NULL};
#elif defined(CONFIG_SOC_LS2K1000)
static char* board_desc_set[] = {LS2K1000_JL_DESC, LS2K1000_DP_DESC, LS2K1000_GBKPDM0_V10_DESC, NULL};
static char* board_name_set[] = {LS2K1000_JL_BOARD_NAME, LS2K1000_DP_BOARD_NAME, LS2K1000_GBKPDM0_V10_BOARD_NAME, NULL};
#elif defined(CONFIG_SOC_LS2P500)
static char* board_desc_set[] = {LS2P500_EVB_DESC, NULL};
static char* board_name_set[] = {LS2P500_EVB_BOARD_NAME, NULL};
#elif defined(CONFIG_SOC_LS2K300)
static char* board_desc_set[] = {LS2K300_MINI_DP_DESC, NULL};
static char* board_name_set[] = {LS2K300_MINI_DP_BOARD_NAME, NULL};
#endif

char* get_board_name_by_desc(char* desc)
{
	int index;
	if (!desc)
		return NULL;
	for (index = 0; ;++index)
	{
		if (!board_desc_set[index])
			return NULL;
		if (!strcmp(board_desc_set[index], desc))
			return board_name_set[index];
	}
	return NULL;
}
