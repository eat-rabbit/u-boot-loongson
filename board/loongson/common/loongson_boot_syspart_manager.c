#include <stdio.h>
#include <common.h>
#include <command.h>
#include <malloc.h>
#include "bdinfo/bdinfo.h"
#include "loongson_boot_syspart_manager.h"

static int setup_syspart_uboot_env(char* syspart)
{
	if (!syspart)
		return -1;
	env_set(SYSPART_FIRST_UBOOT_ENV_NAME, syspart);

	if (!strcmp(syspart, SYSPART_FIRST))
		env_set(SYSPART_SECOND_UBOOT_ENV_NAME, SYSPART_SECOND);
	else
		env_set(SYSPART_SECOND_UBOOT_ENV_NAME, SYSPART_FIRST);

	env_set(SYSPART_USER_CH_UBOOT_ENV_NAME, "0");

	env_save();
	return 0;
}

static int setup_syspart_nvme_env(char* syspart)
{
	if (!syspart)
		return -1;

	bdinfo_set(BDI_ID_SYSPART, syspart);

#ifdef LS_DOUBLE_SYSTEM
	if (!strcmp(syspart, SYSPART_FIRST))
		bdinfo_set(BDI_ID_SYSPART_LAST, SYSPART_SECOND);
	else
		bdinfo_set(BDI_ID_SYSPART_LAST, SYSPART_FIRST);
#endif

	bdinfo_save();
	return 0;
}

int switch_syspart(void)
{
	int ret = 0;
	char *env_val = NULL;

	// from uboot env
	env_val = env_get(SYSPART_FIRST_UBOOT_ENV_NAME);
	if (!env_val)
		return -1;

	if (!strcmp(env_val, SYSPART_FIRST))
		env_val = SYSPART_SECOND;
	else
		env_val = SYSPART_FIRST;

	ret = setup_syspart_uboot_env(env_val);
	if (ret)
		printf("%s: setup uboot env failed! (target: %s)", __func__, env_val);
	ret = setup_syspart_nvme_env(env_val);
	if (ret)
		printf("%s: setup nvme env failed! (target: %s)", __func__, env_val);
	return ret ? -1 : 0;
}

int setup_cur_syspart(char* syspart)
{
	int ret = 0;

	if (!syspart)
		return -1;

	ret = setup_syspart_uboot_env(syspart);
	if (ret)
		printf("%s: setup uboot env failed! (target: %s)\n", __func__, syspart);
	ret = setup_syspart_nvme_env(syspart);
	if (ret)
		printf("%s: setup nvme env failed! (target: %s)\n", __func__, syspart);
	return ret ? -1 : 0;
}

int detect_user_change_syspart(void)
{
	int ret = 0;
	char *env_val = NULL;

	env_val = env_get(SYSPART_USER_CH_UBOOT_ENV_NAME);
	if (!env_val)
		return -1;

	if (!strcmp(env_val, SYSPART_USER_CH_DISABLE))
		return 0;

	env_val = env_get(SYSPART_FIRST_UBOOT_ENV_NAME);
	if (!env_val)
		return -1;

	ret = setup_syspart_nvme_env(env_val);
	if (ret)
		printf("%s: setup nvme env failed! (target: %s)", __func__, env_val);
	return ret ? -1 : 0;
}
