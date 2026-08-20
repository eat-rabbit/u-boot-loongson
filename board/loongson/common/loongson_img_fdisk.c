
#include <common.h>
#include <command.h>
#include <malloc.h>

#define TARGET_FDISK_TXT_PATH "/usr/local/ls_resize/fdisk.txt"

static void try_read_block(char* block_type)
{
	int ret;
#if defined(CONFIG_SOC_LS2K300) || defined(CONFIG_SOC_LS2P500)
	char* target_block = "mmc0:1";
#else
	char* target_block = "scsi0:1";
#endif
	char cmd[256];

	memset(cmd, 0 , 256);
	sprintf(cmd, "general_load --if %s%s --fmt fat32 --sym /update/fdisk.txt --of %s --fmt ext4 --sym %s --force", block_type, "0:1", target_block, TARGET_FDISK_TXT_PATH);
	ret = run_command(cmd, 0);

	if (!ret)
		return;

	memset(cmd, 0 , 256);
	sprintf(cmd, "general_load --if %s%s --fmt fat32 --sym /update/fdisk.txt --of %s --fmt ext4 --sym %s --force", block_type, "0", target_block, TARGET_FDISK_TXT_PATH);
	ret = run_command(cmd, 0);

	if (!ret)
		return;

	memset(cmd, 0 , 256);
	sprintf(cmd, "general_load --if %s%s --fmt ext4 --sym /update/fdisk.txt --of %s --fmt ext4 --sym %s --force", block_type, "0:1", target_block, TARGET_FDISK_TXT_PATH);
	ret = run_command(cmd, 0);

	if (!ret)
		return;

	memset(cmd, 0 , 256);
	sprintf(cmd, "general_load --if %s%s --fmt ext4 --sym /update/fdisk.txt --of %s --fmt ext4 --sym %s --force", block_type, "0", target_block, TARGET_FDISK_TXT_PATH);
	ret = run_command(cmd, 0);

	if (!ret)
		return;
}

static void try_read_tftp(void)
{
	char* server_ip = NULL;
#if defined(CONFIG_SOC_LS2K300) || defined(CONFIG_SOC_LS2P500)
	char* target_block = "mmc0:1";
#else
	char* target_block = "scsi0:1";
#endif
	char cmd[256];
	// general_load --if net:192.168.1.2 --fmt tftp --sym fdisk.txt --of mmc0:1 --fmt ext4 --sym /root/fdisk.txt --force

	server_ip = env_get("serverip");

	// fdisk.txt 是隐匿的，应快速超时
	env_set("tftptimeout", "1");
	env_set("tftptimeoutcountmax", "1");

	memset(cmd, 0 , 256);
	sprintf(cmd, "general_load --if net:%s --fmt tftp --sym fdisk.txt --of %s --fmt ext4 --sym %s --force", server_ip, target_block, TARGET_FDISK_TXT_PATH);
	run_command(cmd, 0);

	return;
}

int loongson_try_read_img_fdisk(char* read_way)
{
	if (!read_way)
		return -1;

	print_enable_setup(0);
	if (!strcmp(read_way, "usb")) {
		try_read_block(read_way);
	} else  if (!strcmp(read_way, "mmc")) {
		try_read_block(read_way);
	} else if (!strcmp(read_way, "tftp")) {
		try_read_tftp();
	}
	print_enable_setup(1);
	return 0;
}
