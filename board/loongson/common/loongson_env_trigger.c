// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <command.h>
#include <malloc.h>
#include "loongson_boot_syspart_manager.h"
#include "loongson_wdt_setup.h"

/*
 * 这是一个模拟uboot启动的时候的会自动复位的触发器逻辑
 * 按照 ls_trigger_boot 这个例子
 * 即 扫描 这个环境变量 如果是默认值，那么就什么事情都不做
 * 如果是 nand 或者 ssd 那么就设置为 对应的启动方式
 * 并且 自动复位 变成默认值
 * 也就是说这个处理逻辑就是设置了值之后只会执行一次
 * 而设置了不是默认值的话，需要 loongson_env_trigger ls_trigger_boot
 * 才会运行上述的处理逻辑
 * 所以在 platform.c 里面
 * 每次开机都 loongson_env_trigger ls_trigger_boot
 * 就可以做到每次开机都扫描，并且设置启动方式(如果设置了nand 或者 ssd 的话)
 *
 * loongson_env_trigger init
 * 则是 对没创建的 环境变量 初始化为默认值
 */

typedef int (*loongson_trigger_handle_func)(int);
typedef void (*loongson_trigger_tip_func)(void);

static int loongson_handle_boot_change(int index);
static void loongson_tip_boot_change(void);

static int loongson_handle_update_kernel_change(int index);
static void loongson_tip_update_kernel_change(void);

static int loongson_handle_update_rootfs_change(int index);
static void loongson_tip_update_rootfs_change(void);

static int loongson_handle_update_uboot_change(int index);
static void loongson_tip_update_uboot_change(void);

static int loongson_handle_ab_sys_status_change(int index);
static void loongson_tip_ab_sys_status_change(void);

static char* trigger_name_list[] = {
	"ls_trigger_boot",
	"ls_trigger_u_kernel",
	"ls_trigger_u_rootfs",
	"ls_trigger_u_uboot",
	"ls_trigger_ab_sys_status",
	NULL,
};
static loongson_trigger_handle_func trigger_handle_func_list[] = {
	loongson_handle_boot_change,
	loongson_handle_update_kernel_change,
	loongson_handle_update_rootfs_change,
	loongson_handle_update_uboot_change,
	loongson_handle_ab_sys_status_change,
	NULL
};
static loongson_trigger_tip_func trigger_tip_func_list[] = {
	loongson_tip_boot_change,
	loongson_tip_update_kernel_change,
	loongson_tip_update_rootfs_change,
	loongson_tip_update_uboot_change,
	loongson_tip_ab_sys_status_change,
	NULL
};
static char* trigger_default_value_list[] = {"0", "0", "0", "0", "0", NULL};

static int trigger_first_check(int index, char* env_value_bak, int buffer_len)
{
	int ret;
	char* env_name;
	char* env_value;
	char* env_default_value;
	char reset_trigger_cmd[64];

	if (!env_value_bak)
		return -1;

	env_name = trigger_name_list[index];
	env_value = env_get(env_name);
	env_default_value = trigger_default_value_list[index];

	memmove(env_value_bak, env_value, buffer_len);
	env_value_bak[buffer_len - 1] = 0;

	if (!strcmp(env_value, env_default_value))
		return 1;

	/*
	 * 担心下面的动作包含重启，所以先复位
	 */
	memset(reset_trigger_cmd, 64, 0);
	sprintf(reset_trigger_cmd, "setenv %s %s;saveenv", env_name, env_default_value);
	reset_trigger_cmd[63] = 0;
	ret = run_command(reset_trigger_cmd, 0);

	return ret ? -1 : 0;
}

static int loongson_handle_boot_change(int index)
{
	int ret;
	char env_value[32];

	ret = trigger_first_check(index, env_value, 32);
	if (ret == -1)
		return -1;
	else if (ret == 1)
		return 0;

	if (!strcmp(env_value, "nand")) {
		#ifdef NAND_BOOT_ENV
			printf("set boot system from nand .....\r\n");
			ret = run_command(NAND_BOOT_ENV, 0);
		#else
			return -1;
		#endif
	} else if (!strcmp(env_value, "ssd")) {
		#ifdef SATA_BOOT_ENV
			printf("set boot system from SSD .....\r\n");
			ret = run_command(SATA_BOOT_ENV, 0);
		#else
			return -1;
		#endif
	} else if (!strcmp(env_value, "mmc")) {
		#ifdef EMMC_BOOT_ENV
			printf("set boot system from mmc0 .....\r\n");
			ret = run_command(EMMC_BOOT_ENV, 0);
		#else
			return -1;
		#endif
	} else if (!strcmp(env_value, "mmc0")) {
		#ifdef EMMC_BOOT_ENV
			printf("set boot system from mmc0 .....\r\n");
			ret = run_command(EMMC_BOOT_ENV, 0);
		#else
			return -1;
		#endif
	} else if (!strcmp(env_value, "mmc1")) {
		#ifdef SDCARD_BOOT_ENV
			printf("set boot system from mmc1 .....\r\n");
			ret = run_command(SDCARD_BOOT_ENV, 0);
		#else
			return -1;
		#endif
	} else
		return -1;

	return ret;
}

static void loongson_tip_boot_change(void)
{
	printf("\tvalue:\n");
	printf("\t\t0    : no handle\n");
	printf("\t\tnand : set boot from nand default\n");
	printf("\t\tssd  : set boot from ssd default\n");
	printf("\t\tmmc  : set boot from mmc0 default\n");
	printf("\t\tmmc0 : set boot from mmc0 default\n");
	printf("\t\tmmc1 : set boot from mmc1 default\n");
	printf("\n");
}

static int loongson_handle_update_kernel_change(int index)
{
	int ret;
	char env_value[32];

	ret = trigger_first_check(index, env_value, 32);
	if (ret == -1)
		return -1;
	else if (ret == 1)
		return 0;

	if (!strcmp(env_value, "nandusb")) {
		printf("update kernel which in nand by usb .....\r\n");
		ret = run_command("loongson_update usb kernel nand", 0);
	} else if (!strcmp(env_value, "nandtftp")) {
		printf("update kernel which in nand by tftp .....\r\n");
		ret = run_command("loongson_update tftp kernel nand", 0);
	} else if (!strcmp(env_value, "ssdusb")) {
		printf("update kernel which in ssd by usb .....\r\n");
		ret = run_command("loongson_update usb kernel sata", 0);
	} else if (!strcmp(env_value, "ssdtftp")) {
		printf("update kernel which in ssd by tftp .....\r\n");
		ret = run_command("loongson_update tftp kernel sata", 0);
	} else
		return -1;

	return ret;
}

static void loongson_tip_update_kernel_change(void)
{
	printf("\tvalue:\n");
	printf("\t\t0   : no handle\n");
	printf("\t\tnandusb:  update kernel which in nand by usb\n");
	printf("\t\tnandtftp: update kernel which in nand by tftp\n");
	printf("\t\tssdusb:   update kernel which in ssd by usb\n");
	printf("\t\tssdtftp:  update kernel which in ssd by tftp\n");
	printf("\n");
}

static int loongson_handle_update_rootfs_change(int index)
{
	int ret;
	char env_value[32];

	ret = trigger_first_check(index, env_value, 32);
	if (ret == -1)
		return -1;
	else if (ret == 1)
		return 0;

	if (!strcmp(env_value, "nandusb")) {
		printf("update rootfs which in nand by usb .....\r\n");
		ret = run_command("loongson_update usb rootfs nand", 0);
	} else if (!strcmp(env_value, "nandtftp")) {
		printf("update rootfs which in nand by tftp .....\r\n");
		ret = run_command("loongson_update tftp rootfs nand", 0);
	} else if (!strcmp(env_value, "ssdusb")) {
		printf("update rootfs which in sata by tftp .....\r\n");
		ret = run_command("recover usb", 0);
	} else if (!strcmp(env_value, "ssdtftp")) {
		printf("update rootfs which in sata by tftp .....\r\n");
		ret = run_command("recover tftp", 0);
	}
	else
		return -1;

	return ret;
}

static void loongson_tip_update_rootfs_change(void)
{
	printf("\tvalue:\n");
	printf("\t\t0   : no handle\n");
	printf("\t\tnandusb:  update rootfs which in nand by usb\n");
	printf("\t\tnandtftp: update rootfs which in nand by tftp\n");
	printf("\t\tssdusb:   update rootfs which in sata by usb\n");
	printf("\t\tssdtftp:  update rootfs which in sata by tftp\n");
	printf("\n");
}

static int loongson_handle_update_uboot_change(int index)
{
	int ret;
	char env_value[32];

	ret = trigger_first_check(index, env_value, 32);
	if (ret == -1)
		return -1;
	else if (ret == 1)
		return 0;

	if (!strcmp(env_value, "usb")) {
		printf("update uboot by usb .....\r\n");
		ret = run_command("loongson_update usb uboot", 0);
	} else if (!strcmp(env_value, "tftp")) {
		printf("update uboot by tftp .....\r\n");
		ret = run_command("loongson_update tftp uboot", 0);
	} else
		return -1;

	ret = run_command("reset", 0);

	return ret;
}

static void loongson_tip_update_uboot_change(void)
{
	printf("\tvalue:\n");
	printf("\t\t0   : no handle\n");
	printf("\t\tusb:  update uboot by usb\n");
	printf("\t\ttftp: update uboot by tftp\n");
	printf("\n");
}

static int loongson_handle_ab_sys_status_change(int index)
{
	int ret;
	char env_value[32];

	ret = trigger_first_check(index, env_value, 32);
	if (ret == -1)
		return -1;
	else if (ret == 1)
		return 0;

	ret = 0;
	if (!strcmp(env_value, "boot1")) {
		printf("Detected new system ...\n");
		// setup env ls_trigger_ab_sys_status boot2
		ret = run_command("setenv ls_trigger_ab_sys_status boot2;saveenv", 0);
	} else if (!strcmp(env_value, "boot1_wdt")) {
		printf("Detected new system (with boot failed detect)...\n");
		// setup watchdog 34s 2k max is 34 s
		ls_wdt_start(LS_WDT_DEFAULT_TIMEOUT_MS);
		// setup env ls_trigger_ab_sys_status boot2
		ret = run_command("setenv ls_trigger_ab_sys_status boot2;saveenv", 0);
	} else if (!strcmp(env_value, "boot2")) {
		printf("Detected new system boot failed! run back to last system...\n");
		// switch last boot part
		switch_syspart();
		ret = run_command("setenv ls_trigger_ab_sys_status boot4;saveenv", 0);
	} else if (!strcmp(env_value, "boot4")) {
		// just wait last part start... and need set this value to be 0
		ret = 0;
	} else
		return -1;

	return ret;
}

static void loongson_tip_ab_sys_status_change(void)
{
	printf("\tvalue:\n");
	printf("\t\t0       : no handle\n");
	printf("\t\tboot1   : new system first boot(uboot will change it to be boot2)\n");
	printf("\t\boot1_wdt: new system first boot and use watchdog detect auto run back(uboot will change it to be boot2)\n");
	printf("\t\tboot2   : new system first boot failed(system need set this value to be 0, either uboot will boot last disk)\n");
	printf("\t\boot4    : to old system known install ab system failed\n");
	printf("\n");
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////////底层逻辑实现////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void do_loongson_env_trigger_error_tip(void)
{
	int i;
	printf("trigger:\n");
	for (i = 0; ; ++i) {
		if (!trigger_name_list[i])
			break;
		printf("\t%s\n", trigger_name_list[i]);
		//usage
		(trigger_tip_func_list[i])();
	}
}

static void loongson_trigger_init(void)
{
	int index;
	int save_target = 0;
	char* env_val = NULL;
	char setenv_cmd[64];

	index = 0;
	for (index = 0; ; ++index) {
		if (!trigger_name_list[index])
			break;

		memset(setenv_cmd, 0 , 64);
		env_val = env_get(trigger_name_list[index]);
		if (!env_val) {
			printf("init %s and set it default value\n", trigger_name_list[index]);
			sprintf(setenv_cmd, "setenv %s 0", trigger_name_list[index]);
			run_command(setenv_cmd, 0);
			save_target = 1;
		} else {
			if (strcmp(env_val, trigger_default_value_list[index]))
				continue;
		}
	}
	if(save_target)
		run_command("saveenv", 0);
}

static int do_loongson_env_trigger(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = -1;
	int index;

	if (argc != 2)
		goto do_error;

	if (!argv[1])
		goto do_error;

	if (!strcmp(argv[1], "init")) {
		loongson_trigger_init();
		ret = 0;
	} else {
		index = 0;
		for (index = 0; ; ++index) {
			if (!trigger_name_list[index])
				break;
			if (!strcmp(argv[1], trigger_name_list[index])) {
				ret = (trigger_handle_func_list[index])(index);
				break;
			}
		}
	}

	if (ret)
		goto do_error;

	return ret;
do_error:
	do_loongson_env_trigger_error_tip();
	return ret;
}

#define LOONGSON_ENV_TRIGGER_HELP_HEAD "handle some oneshot action. for rootfs change uboot interface"

#define LOONGSON_ENV_TRIGGER_USAGE_HEAD "<option>\n"

#define LOONGSON_ENV_TRIGGER_USAGE_OPTION "option: is all ls_trigger_* env\n"
#define LOONGSON_ENV_TRIGGER_USAGE_INIT   "        if option is init will create all ls_trigger_* if not exist\n"

#define LOONGSON_ENV_TRIGGER_HELP LOONGSON_ENV_TRIGGER_HELP_HEAD

#define LOONGSON_ENV_TRIGGER_USAGE LOONGSON_ENV_TRIGGER_USAGE_HEAD \
							LOONGSON_ENV_TRIGGER_USAGE_OPTION \
							LOONGSON_ENV_TRIGGER_USAGE_INIT

U_BOOT_CMD(
	loongson_env_trigger,    2,    1,     do_loongson_env_trigger,
	LOONGSON_ENV_TRIGGER_HELP,
	LOONGSON_ENV_TRIGGER_USAGE
);
