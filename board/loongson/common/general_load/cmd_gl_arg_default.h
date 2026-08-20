// WARN: only included by cmd_gl_arg.c

struct cmd_gl_default_s
{
	const char* net_proto;
	const char* blk_prefix;
	const char* boot_device;
	const char* root_device;
	const char* kernel_device;
	const char* kernel_fstype;
	const char* kernel_prefix;
};

const static struct cmd_gl_default_s __default =
#if defined(CONFIG_SOC_LS2K300) || defined(CONFIG_SOC_LS2P500)
{
	.net_proto = "tftp",
	.blk_prefix = "/update/",
	.boot_device = "flash0:1",
	.root_device = "mmc0",
	.kernel_device = "mmc0:1",
	.kernel_fstype = "ext4",
	.kernel_prefix = "/boot/",
}
#else
{
	.net_proto = "tftp",
	.blk_prefix = "/update/",
	.boot_device = "flash0:1",
	.root_device = "scsi0",
	.kernel_device = "scsi0:1",
	.kernel_fstype = "ext4",
	.kernel_prefix = "/boot/",
}
#endif
;

#define default_net_proto __default.net_proto
#define default_blk_prefix __default.blk_prefix
#define default_boot_device __default.boot_device
#define default_root_device __default.root_device
#define default_kernel_device __default.kernel_device
#define default_kernel_fstype __default.kernel_fstype
#define default_kernel_prefix __default.kernel_prefix


