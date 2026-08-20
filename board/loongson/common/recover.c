// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <command.h>
#include <stdlib.h>

#include <asm/gpio.h>
#include "loongson_storage_read_file.h"
#include "bdinfo/bdinfo.h"
#include "loongson_boot_syspart_manager.h"

typedef enum recover_network {
	NETWORK_TFTP = 0,
	NETWORK_DHCP,
	NETWORK_UNKNOWN,
} recover_network;

// 0 is scsi default
// 1 is mmc
static int install_target=0;

static int run_recover_cmd(char *cmd)
{
	int ret = -1;
	char cmdbuf[CONFIG_SYS_CBSIZE];	/* working copy of cmd		*/
	char *token;			/* start of token in cmdbuf	*/
	char *sep;			/* end of token (separator) in cmdbuf */
	char *str = cmdbuf;
	int inquotes;

	strcpy(cmdbuf, cmd);

	while (*str) {
		/*
		 * Find separator, or string end
		 * Allow simple escape of ';' by writing "\;"
		 */
		for (inquotes = 0, sep = str; *sep; sep++) {
			if ((*sep == '\'') &&
			    (*(sep - 1) != '\\'))
				inquotes = !inquotes;

			if (!inquotes &&
			    (*sep == ';') &&	/* separator		*/
			    (sep != str) &&	/* past string start	*/
			    (*(sep - 1) != '\\'))	/* and NOT escaped */
				break;
		}

		/*
		 * Limit the token to data between separators
		 */
		token = str;
		if (*sep) {
			str = sep + 1;	/* start of command for next pass */
			*sep = '\0';
		} else {
			str = sep;	/* no more commands for next pass */
		}

		ret = run_command(token, 0);
		if (ret) {
			printf("token: \"%s\" faile\n", token);
			break;
		}
	}

	return ret;
}

static int run_recover_cmd_for_storage(enum if_type if_type)
{
	int ret = -1;
	char cmdbuf[256];	/* working copy of cmd */
	int last_devid;
	int last_partition;
	int i;
	char* type;
	enum if_type if_type_set[] = {IF_TYPE_USB, IF_TYPE_MMC};
	char* type_set[] = {"usb", "mmc", NULL};
	char* ins_target_set[] = {"scsi", "mmc", NULL};
	int status;

	status = 0;
	type = NULL;
	for (i = 0; ; ++i) {
		if (!type_set[i])
			break;
		if (if_type == if_type_set[i])
			type = type_set[i];
	}
	if (!type)
		return -1;

	//usb reset mmc dont reset
	if (if_type == IF_TYPE_USB) {
		ret = run_command("usb reset", 0);
		if (ret) {
			status = 1;
			goto reset_failed;
		}
	}

	//read kernel
	ret = storage_read_file(if_type, "${loadaddr}", "/install/uImage", 0, &last_devid, &last_partition);
	if (ret) {
		status = 2;
		goto reset_failed;
	}
	//read ramdisk
	ret = storage_read_file(if_type, "${rd_start}", "/install/ramdisk.gz", 1, &last_devid, &last_partition);
	if (ret) {
		status = 2;
		goto reset_failed;
	}

	// set bootargs env
	ret = run_command(RECOVER_FRONT_BOOTARGS, 0);
	if (ret) {
		status = 3;
		goto reset_start_failed;
	};

	// set bootargs env by loongson env
	memset(cmdbuf, 0, 256);
	sprintf(cmdbuf, "setenv bootargs ${bootargs} ins_way=%s ins_target=%s;", type, ins_target_set[install_target]);
	ret = run_command(cmdbuf, 0);
	if (ret) {
		status = 3;
		goto reset_start_failed;
	}

	// boot kernel and ramdisk.gz
	ret = run_command(RECOVER_START, 0);
	if (ret) {
		status = 3;
		goto reset_start_failed;
	}
	return ret;

	printf("##################################################################\n");
reset_failed:
	if (status == 2)
		printf("### Error %s storage not found kernel or ramdisk.gz\n", type);
reset_start_failed:
	printf("##################################################################\n");
	return ret;
}

/*
 * network_way
 * 0 tftp
 * 1 dhcp
 */
static int run_recover_cmd_for_network(recover_network network_way)
{
	int ret = -1;
	char cmdbuf[256];	/* working copy of cmd */
	char* ins_target_set[] = {"scsi", "mmc", NULL};
	char* download_cmd[] = {RECOVER_TFTP_DOWNLOAD_CMD, RECOVER_DHCP_DOWNLOAD_CMD, NULL};
	int status;

	status = 0;

	if (network_way >= NETWORK_UNKNOWN) {
		status = 2;
		goto reset_failed;
	}

	// read kernel and ramdisk
	ret = run_command(download_cmd[network_way], 0);
	if (ret) {
		status = 2;
		goto reset_failed;
	}

	// set bootargs env
	ret = run_command(RECOVER_FRONT_BOOTARGS, 0);
	if (ret) {
		status = 3;
		goto reset_failed;
	};

	// set bootargs env by loongson env
	memset(cmdbuf, 0, 256);
	sprintf(cmdbuf, "setenv bootargs ${bootargs} ins_way=tftp ins_target=%s u_ip=${ipaddr} u_sip=${serverip};", ins_target_set[install_target]);
	ret = run_command(cmdbuf, 0);
	if (ret) {
		status = 3;
		goto reset_failed;
	}

	// boot kernel and ramdisk.gz
	ret = run_command(RECOVER_START, 0);
	if (ret) {
		status = 3;
		goto reset_failed;
	}
	return ret;

reset_failed:
	printf("##################################################################\n");
	if (status == 2)
		printf("### Error download kernel or ramdisk.gz\n");
	printf("##################################################################\n");
	return ret;
}

static int do_recover_from_usb(void)
{
	printf("Install System By USB .....\r\n");
	// return run_recover_cmd(RECOVER_USB_DEFAULT);
	return run_recover_cmd_for_storage(IF_TYPE_USB);
}

static int do_recover_from_tftp(void)
{
	// char cmd[]= "tftpboot ${loadaddr} uImage;tftpboot ${rd_start} ramdisk.gz;"RECOVER_DEFAULT_ENV"";
	printf("Install System By tftp .....\r\n");
	return run_recover_cmd_for_network(NETWORK_TFTP);
}

static int do_recover_from_dhcp(void)
{
	printf("Install System By dhcp .....\r\n");
	return run_recover_cmd_for_network(NETWORK_DHCP);
}

#ifdef CONFIG_MMC
static int do_recover_from_mmc(void)
{
	printf("Install System By MMC .....\r\n");
	// return run_recover_cmd(RECOVER_MMC_DEFAULT);
	return run_recover_cmd_for_storage(IF_TYPE_MMC);
}
#endif

static int do_recover_from_sata(char *part_id)
{
#ifdef RECOVER_SATA_DEFAULT
	printf("Recover System By SATA .....\r\n");
	return run_recover_cmd(RECOVER_SATA_DEFAULT);
#else
	char cmd[CONFIG_SYS_CBSIZE] = {0};
	snprintf(cmd, sizeof(cmd), "scsi reset;ext4load scsi 0:%s ${loadaddr} uImage;ext4load scsi 0:%s ${rd_start} ramdisk.gz;%s setenv bootargs ${bootargs} rec_sys=1; %s"
		, part_id, part_id, RECOVER_FRONT_BOOTARGS, RECOVER_START);
	return run_recover_cmd(cmd);
#endif
}

static int do_recover_from_mmc_r(char *part_id)
{
#ifdef RECOVER_SATA_DEFAULT
	printf("Recover System By SATA .....\r\n");
	return run_recover_cmd(RECOVER_SATA_DEFAULT);
#else
	char cmd[CONFIG_SYS_CBSIZE] = {0};
	snprintf(cmd, sizeof(cmd), "ext4load mmc 0:%s ${loadaddr} uImage;ext4load mmc 0:%s ${rd_start} ramdisk.gz;%s setenv bootargs ${bootargs} rec_sys=1 rec_target=mmc; %s"
		, part_id, part_id, RECOVER_FRONT_BOOTARGS, RECOVER_START);
	return run_recover_cmd(cmd);
#endif
}

/* 上电时长按按钮3秒进入recover功能, recover优先顺序usb>mmc>sata
   按键使用 LS_RECOVER_GPIO_BUTTON 定义的gpio */
int recover(void)
{
#ifdef LS_RECOVER_GPIO_BUTTON
	int ret;
	int timeout = 0;

	gpio_request(LS_RECOVER_GPIO_BUTTON, "recover_butt");
	gpio_direction_input(LS_RECOVER_GPIO_BUTTON);

	while (gpio_get_value(LS_RECOVER_GPIO_BUTTON) == 0) {
		mdelay(10);
		timeout++;
		if (timeout > 300) {
			if (do_recover_from_usb() == 0) {
			} else if (do_recover_from_mmc() == 0) {
			} else {
				char part_id[4] = "4";
				do_recover_from_sata(part_id);
			}
			break;
		}
	}
#endif
	return 0;
}

#ifdef LS_DOUBLE_SYSTEM

static int do_recover_to_last(void)
{
	char *env;
	int ret;

	env = env_get("bootcmd");
	if (!strstr(env, "scsi") && !strstr(env, "mmc") ) {
		printf("current maybe is boot by nand\n");
		return -1;
	}

	ret = switch_syspart();

	return ret;
}
#endif

/*
* recover_cmd usb . recover from usb
* recover_cmd sata. recover from sata
* recover_cmd sata 3. recover from part 3 of sata
*
* default table
* /dev/sda1 /
* /dev/sda2 /data
* /dev/sda3 swap
* /dev/sda4 backup
*
*/
static int do_recover_cmd(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = -1;
	if (argc < 2) {
		return ret;
	}

	install_target = 0;
	if (!argv[3])
		install_target = 0;
	else {
		if (!strcmp(argv[3], "scsi"))
			install_target = 0;
		else if (!strcmp(argv[3], "mmc"))
			install_target = 1;
	}

	if (strcmp(argv[1], "usb") == 0) {
		ret = do_recover_from_usb();
	}
	else if (strcmp(argv[1], "tftp") == 0) {
		ret = do_recover_from_tftp();
	}
	else if (strcmp(argv[1], "dhcp") == 0) {
		ret = do_recover_from_dhcp();
	}
#ifdef CONFIG_MMC
	else if (strcmp(argv[1], "mmc") == 0) {
		ret = do_recover_from_mmc();
	}
#endif
	else if (strcmp(argv[1], "sata") == 0) {
		char part_id[4] = "4";
		if (argc == 3) {
			memset(part_id, 0, sizeof(part_id));
			int len = strlen(argv[2]) > sizeof(part_id) ? sizeof(part_id) : strlen(argv[2]);
			strncpy(part_id, argv[2], len);
		}
		printf("part_id:%s\n", part_id);
		ret = do_recover_from_sata(part_id);
	}
	else if (strcmp(argv[1], "mmc_r") == 0) {
		char part_id[4] = "4";
		if (argc == 3) {
			memset(part_id, 0, sizeof(part_id));
			int len = strlen(argv[2]) > sizeof(part_id) ? sizeof(part_id) : strlen(argv[2]);
			strncpy(part_id, argv[2], len);
		}
		printf("part_id:%s\n", part_id);
		ret = do_recover_from_mmc_r(part_id);
	}
#ifdef LS_DOUBLE_SYSTEM
	else if (strcmp(argv[1], "last") == 0) {
		ret = do_recover_to_last();
	}
#endif

	return ret;
}

#define RECOVER_CMD_TIP_HEAD "recover system by usb or backup partition.\n"\
								"recover_cmd <option> [part_id] [install_target]\n"\
								"option1: usb: recover from usb\n"\
								"        tftp: recover from tftp\n"

#ifdef CONFIG_MMC
#define RECOVER_CMD_TIP_MMC "         mmc: recover from mmc\n"
#else
#define RECOVER_CMD_TIP_MMC ""
#endif

#ifdef LS_DOUBLE_SYSTEM
#define RECOVER_CMD_TIP_LAST "        last: recover to last time boot system\n"
#else
#define RECOVER_CMD_TIP_LAST ""
#endif

#ifdef CONFIG_LOONGSON_BOARD_SATA_FS
#define INSTALL_TARGET_SCSI_TIP "option3: scsi: install system to sda"
#else
#define INSTALL_TARGET_SCSI_TIP ""
#endif

#ifdef CONFIG_LOONGSON_BOARD_MMC_FS
#define INSTALL_TARGET_MMC_TIP "option3: mmc : install system to mmc"
#else
#define INSTALL_TARGET_MMC_TIP ""
#endif

#define RECOVER_CMD_TIP RECOVER_CMD_TIP_HEAD \
							RECOVER_CMD_TIP_MMC \
							RECOVER_CMD_TIP_LAST \
							"         sata: recover from sata\n"\
							"         mmc_r: recover from mmc ext4 part\n"\
							"option2: part_id: recover from part id\n" \
							INSTALL_TARGET_SCSI_TIP \
							INSTALL_TARGET_MMC_TIP

U_BOOT_CMD(
	recover_cmd,    4,    1,     do_recover_cmd,
	"recover the system",
	RECOVER_CMD_TIP
);

