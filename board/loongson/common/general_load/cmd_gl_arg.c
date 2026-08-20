#include <net.h>
#include <part.h>
#include <fs.h>
#include <command.h>
#include "cmd_gl_arg.h"
#include "cmd_gl_arg_default.h"

#define MAX_CMD_GL_ARGLEN 32

typedef struct cmd_gl_arg {
	char device[MAX_CMD_GL_ARGLEN];
	char fmt[MAX_CMD_GL_ARGLEN];
	char sym[MAX_CMD_GL_ARGLEN];
} cmd_gl_arg_t;

static __inline__ char* find_last_char(char* str, char c)
{
	char* p;
	char* last = NULL;
	for (p = str; *p != '\0'; p++)
		if (*p == c)
			last = p;

	return last;
}

static bool strip_file_path(char* file, char* path)
{
	char* last = find_last_char(path, '/');

	if (last == NULL)
	{
		strcpy(file, path);
		return false;
	}
	else
	{
		strcpy(file, last + 1);
		return true;
	}
}

static int load_from_external_device(cmd_gl_arg_t* arg, char* filename)
{
	if (strncmp(arg->device, "net", 3) == 0) {
		if(arg->device[3] == '\0')
		{
			char* serverip = env_get("serverip");
			arg->device[3] = ':';
			strcpy(arg->device + 4, serverip);
		}
		if (arg->fmt[0] == '\0')
			strcpy(arg->fmt, default_net_proto);
	} else if (strncmp(arg->device, "usb", 3) == 0) {
		if(arg->device[3] == '\0')
		{
			struct blk_desc* desc;

			run_command("usb reset", 0);

			desc = blk_get_devnum_by_type(IF_TYPE_USB, 0);
			if (!desc)
			{
				printf("device ERROR: not found usb device %s\n", arg->device);
				return -1;
			}
			if (desc->part_type == PART_TYPE_UNKNOWN)
				strcpy(arg->device + 3, "0");
			else
				strcpy(arg->device + 3, "0:1");
		}
		if(arg->fmt[0] == '\0')
		{
			fs_set_blk_dev("usb", arg->device + 3, FS_TYPE_ANY);
			strcpy(arg->fmt, fs_get_type_name());
		}
		snprintf(arg->sym, sizeof(arg->sym),
				"%s%s", default_blk_prefix, filename);
	} else if (strncmp(arg->device, "mmc", 3) == 0) {
		if(arg->device[3] == '\0')
		{
			// Choose externel mmcX
			// We must have a "Device Table" for every board
			// In some board, externel mmc is mmc0
			// But in some board, external mmc is mmc1
			// This "Device Table" will also work on boot-select

			// For now, check mmc1 partition only
		}
		else if(arg->device[4] == '\0')
		{
			struct blk_desc* desc;
			desc = blk_get_devnum_by_type(IF_TYPE_MMC, 1);
			if (!desc)
			{
				printf("device ERROR: not found mmc device %s\n", arg->device);
				return -1;
			}
			if (desc->part_type != PART_TYPE_UNKNOWN)
				strcpy(arg->device + 4, ":1");
		}
		if(arg->fmt[0] == '\0')
		{
			fs_set_blk_dev("mmc", arg->device + 3, FS_TYPE_ANY);
			strcpy(arg->fmt, fs_get_type_name());
		}
		snprintf(arg->sym, sizeof(arg->sym),
				"%s%s", default_blk_prefix, filename);
	} else {
		printf("Semantic ERROR: external device unknow: %s\n", arg->device);
		return -1;
	}

	return 0;
}

static int cmd_gl_arg_syntax(char** arg, int n,
		cmd_gl_arg_t* if_arg, cmd_gl_arg_t* of_arg,
		bool* force, enum gl_extra_e* extra)
{
	char** a = arg;
	cmd_gl_arg_t* t = if_arg;

parse_opt:
	if (a > arg + (n-1) )
		return 0;

	if (strcmp(*a, "--if") == 0) {
		t = if_arg;
		a++;
		goto parse_device;
	}
	else if (strcmp(*a, "--of") == 0) {
		t = of_arg;
		a++;
		goto parse_device;
	}
	else if ((strcmp(*a, "--fmt") == 0)) {
		a++;
		goto parse_fmt;
	}
	else if ((strcmp(*a, "--sym") == 0)) {
		a++;
		goto parse_symbol;
	}
	else if ((strcmp(*a, "--decompress") == 0)) {
		goto indicate_decompress;
	}
	else if ((strcmp(*a, "--ubootsecure") == 0)) {
		goto indicate_ubootsecure;
	}
	else if ((strcmp(*a, "--force") == 0)) {
		goto force_run;
	}
	else {
		printf("Syntax Error --opt unknown %s\n", *a);
		return -1;
	}

parse_device:
	if (a > arg + (n-1) )
	{
		printf("Syntax Error: --from/to need parameter\n");
		return -1;
	}
	strcpy(t->device, *a);
	a++;
	goto parse_opt;

parse_fmt:
	if (a > arg + (n-1) )
	{
		printf("Syntax Error: --fmt need parameter\n");
		return -1;
	}
	strcpy(t->fmt, *a);
	a++;
	goto parse_opt;

parse_symbol:
	if (a > arg + (n-1) )
	{
		printf("Syntax Error: --sym need parameter\n");
		return 0;
	}
	strcpy(t->sym, *a);
	a++;
	goto parse_opt;

indicate_decompress:
	*extra |= GL_EXTRA_DECOMPRESS;
	a++;
	goto parse_opt;

indicate_ubootsecure:
	*extra |= GL_EXTRA_UBOOTSECURE;
	a++;
	goto parse_opt;

force_run:
	*force = true;
	a++;
	goto parse_opt;
}

// 语义解析结构
// {
//	目标严格（补充参数，且修正已给定的参数）
//	命令简短
//		gl uImage, gl rootfs ...
//	{
//		加载内核
//		升级内核
//		升级系统
//		升级固件
//	}
//
//	***折中方案暂缺（在一定范围内补充参数，但不对已有参数修正）
//	{
//
//	}
//
//	完全宽泛（不做任何解析）
//	命令冗长
//		gl from xxx fmt xxx sym xxx to xxx fmt xxx sym xxx
//	{
//		任何目的
//	}
// }
static int cmd_gl_arg_semantic(cmd_gl_arg_t* if_arg, cmd_gl_arg_t* of_arg, bool force)
{
	char sym[MAX_CMD_GL_ARGLEN];
	cmd_gl_arg_t* arg;

	if(force)
		return 0;

	if (if_arg->sym[0] == '\0')
	{
		printf("Semantic ERROR: if-symbol must not be NULL\n");
		return -1;
	}

	strip_file_path(sym, if_arg->sym);

	if (strncmp(sym, "uImage", 6) == 0)
	{
		goto load_kernel;
	}
	else if (strncmp(sym, "rootfs", 5) == 0)
	{
		goto update_rootfs;
	}
	else if (strncmp(sym, "u-boot", 6) == 0)
	{
		goto update_uboot;
	}
	else
	{
		printf("Semantic unrecognised, normal loading...\n");
		return 0;
	}

load_kernel:
	#define load_kernel_from_internal \
		(if_arg->device[0] == '\0' || \
		 strcmp(if_arg->device, default_kernel_device) == 0)
	if (load_kernel_from_internal)
	{
		arg = if_arg;
		strcpy(of_arg->device, "ram");
	}
	else
	{
		arg = of_arg;
		load_from_external_device(if_arg, sym);
	}
	strcpy(arg->device, (char*)default_kernel_device);
	strcpy(arg->fmt, (char*)default_kernel_fstype);
	snprintf(arg->sym, sizeof(arg->sym),
			"%s%s", default_kernel_prefix, sym);
	return 0;

update_rootfs:
	strcpy(of_arg->device, (char*)default_root_device);
	goto set_external_device;
update_uboot:
	strcpy(of_arg->device, (char*)default_boot_device);
	goto set_external_device;
set_external_device:
	load_from_external_device(if_arg, sym);
	of_arg->sym[0] = '\0';
	of_arg->fmt[0] = '\0';

	return 0;
}

static int cmd_gl_arg_target(cmd_gl_arg_t* arg, gl_target_t* target)
{
	int fmt = FS_TYPE_ANY;

	if (gl_target_set_device(target, arg->device))
	{
		printf("Error: device %s set fail\n", arg->device);
		return -1;
	}

	if (arg->fmt[0] != '\0')
	{
		if (strcmp(arg->fmt, "tftp") == 0)
		{
			fmt = TFTPGET;
		}
#ifdef CONFIG_CMD_DHCP
		else if (strcmp(arg->fmt, "dhcp") == 0) {
			fmt = DHCP;
		}
#endif
		else if (strcmp(arg->fmt, "ext4") == 0) {
			fmt = FS_TYPE_EXT;
		}
		else if (strcmp(arg->fmt, "vfat") == 0 ||
				strcmp(arg->fmt, "fat32") == 0 ||
				strcmp(arg->fmt, "fat") == 0) {
			fmt = FS_TYPE_FAT;
		}
		else
		{
			printf("Error: fmt %s not supported\n", arg->fmt);
			return -1;
		}
		gl_target_set_fmt(target, fmt);

	}

	if (arg->sym[0] != '\0')
		gl_target_set_symbol(target, arg->sym);

	return 0;
}

int cmd_gl_parse(char** arg, int n,
		gl_target_t* src, gl_target_t* dest, enum gl_extra_e* extra)
{
	bool force = false;
	cmd_gl_arg_t if_arg = {0};
	cmd_gl_arg_t of_arg = {0};

	if (cmd_gl_arg_syntax((char**)arg, n,
				&if_arg, &of_arg, &force, extra))
		return -1;

	if (cmd_gl_arg_semantic(&if_arg, &of_arg, force))
		return -1;

	printf("--if %s --fmt %s --sym %s\n",
			if_arg.device, if_arg.fmt, if_arg.sym);
	printf("--of %s --fmt %s --sym %s\n",
			of_arg.device, of_arg.fmt, of_arg.sym);
	printf("--extra 0x%x\n", *extra);

	if (cmd_gl_arg_target(&if_arg, src))
		return -1;

	if (cmd_gl_arg_target(&of_arg, dest))
		return -1;

	return 0;
}

