// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <bootstage.h>
#include <env.h>
#include <image.h>
#include <fdt_support.h>
#include <lmb.h>
#include <log.h>
#include <asm/addrspace.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <stdlib.h>
#include <dm.h>

#if defined(CONFIG_LOONGSON_BOOT_FIXUP)
extern void *build_boot_param(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

#define	LINUX_MAX_ENVS		256
#define	LINUX_MAX_ARGS		256

static int linux_argc;
static char **linux_argv;
static char *linux_argp;

// 新的传参规范里面无需把 command line 拆开成一个一个 直接字符串传进去就好
static char *linux_command_line;

static char **linux_env;
static char *linux_env_p;
static int linux_env_idx;

static ulong arch_get_sp(void)
{
	ulong ret;

	__asm__ __volatile__("move %0, $sp" : "=r"(ret) : );

	return ret;
}

void arch_lmb_reserve(struct lmb *lmb)
{
	ulong sp;

	sp = arch_get_sp();
	debug("## Current stack ends at 0x%08lx\n", sp);

	/* adjust sp by 4K to be safe */
	sp -= 4096;
	lmb_reserve(lmb, sp, gd->ram_top - sp);
}

static void linux_cmdline_init(void)
{
	linux_argc = 1;
	linux_argv = (char **)map_sysmem(gd->bd->bi_boot_params, 0);
	linux_argv[0] = 0;
	linux_argp = (char *)(linux_argv + LINUX_MAX_ARGS);
}

static void linux_cmdline_set(const char *value, size_t len)
{
	linux_argv[linux_argc] = linux_argp;
	memcpy(linux_argp, value, len);
	linux_argp[len] = 0;

	linux_argp += len + 1;
	linux_argc++;
}

static void linux_cmdline_dump(void)
{
	int i;

	debug("## cmdline argv at 0x%p, argp at 0x%p\n",
	      linux_argv, linux_argp);

	for (i = 1; i < linux_argc; i++)
		debug("   arg %03d: %s\n", i, linux_argv[i]);
}

static void linux_cmdline_legacy(bootm_headers_t *images)
{
	const char *bootargs, *next, *quote;

	linux_cmdline_init();

	bootargs = env_get("bootargs");
	if (!bootargs)
		return;

	next = bootargs;

	while (bootargs && *bootargs && linux_argc < LINUX_MAX_ARGS) {
		quote = strchr(bootargs, '"');
		next = strchr(bootargs, ' ');

		while (next && quote && quote < next) {
			/*
			 * we found a left quote before the next blank
			 * now we have to find the matching right quote
			 */
			next = strchr(quote + 1, '"');
			if (next) {
				quote = strchr(next + 1, '"');
				next = strchr(next + 1, ' ');
			}
		}

		if (!next)
			next = bootargs + strlen(bootargs);

		linux_cmdline_set(bootargs, next - bootargs);

		if (*next)
			next++;

		bootargs = next;
	}
}

// static void linux_cmdline_append(bootm_headers_t *images)
// {
// 	char buf[24];
// 	ulong mem, rd_start, rd_size;

// 	/* append mem */
// 	mem = gd->ram_size >> 20;
// 	sprintf(buf, "mem=%luM", mem);
// 	linux_cmdline_set(buf, strlen(buf));

// 	/* append rd_start and rd_size */
// 	rd_start = images->initrd_start;
// 	rd_size = images->initrd_end - images->initrd_start;

// 	if (rd_size) {
// 		sprintf(buf, "rd_start=0x%08lX", rd_start);
// 		linux_cmdline_set(buf, strlen(buf));
// 		sprintf(buf, "rd_size=0x%lX", rd_size);
// 		linux_cmdline_set(buf, strlen(buf));
// 	}
// }

static void linux_env_init(void)
{
	linux_env = (char **)(((ulong) linux_argp + 15) & ~15);
	linux_env[0] = 0;
	linux_env_p = (char *)(linux_env + LINUX_MAX_ENVS);
	linux_env_idx = 0;
}

static void linux_env_set(const char *env_name, const char *env_val)
{
	if (linux_env_idx < LINUX_MAX_ENVS - 1) {
		linux_env[linux_env_idx] = linux_env_p;

		strcpy(linux_env_p, env_name);
		linux_env_p += strlen(env_name);
		*linux_env_p++ = '=';

		strcpy(linux_env_p, env_val);
		linux_env_p += strlen(env_val);

		linux_env_p++;
		linux_env[++linux_env_idx] = 0;
	}
}

static void linux_env_legacy(bootm_headers_t *images)
{
	char env_buf[12];
	const char *cp;
	ulong rd_start, rd_size;

	if (CONFIG_IS_ENABLED(MEMSIZE_IN_BYTES)) {
		sprintf(env_buf, "%lu", (ulong)gd->ram_size);
		debug("## Giving linux memsize in bytes, %lu\n",
		      (ulong)gd->ram_size);
	} else {
		sprintf(env_buf, "%lu", (ulong)(gd->ram_size >> 20));
		debug("## Giving linux memsize in MB, %lu\n",
		      (ulong)(gd->ram_size >> 20));
	}

	rd_start = (ulong)map_sysmem(images->initrd_start, 0);
	rd_size = images->initrd_end - images->initrd_start;

	linux_env_init();

	linux_env_set("memsize", env_buf);

	sprintf(env_buf, "0x%08lX", rd_start);
	linux_env_set("initrd_start", env_buf);

	sprintf(env_buf, "0x%lX", rd_size);
	linux_env_set("initrd_size", env_buf);

	sprintf(env_buf, "0x%08X", (uint) (gd->bd->bi_flashstart));
	linux_env_set("flash_start", env_buf);

	sprintf(env_buf, "0x%X", (uint) (gd->bd->bi_flashsize));
	linux_env_set("flash_size", env_buf);

	cp = env_get("ethaddr");
	if (cp)
		linux_env_set("ethaddr", cp);

	cp = env_get("eth1addr");
	if (cp)
		linux_env_set("eth1addr", cp);
}

static int boot_reloc_fdt(bootm_headers_t *images)
{
	/*
	 * In case of legacy uImage's, relocation of FDT is already done
	 * by do_bootm_states() and should not repeated in 'bootm prep'.
	 */
	if (images->state & BOOTM_STATE_FDT) {
		debug("## FDT already relocated\n");
		return 0;
	}

#if CONFIG_IS_ENABLED(OF_LIBFDT)
	boot_fdt_add_mem_rsv_regions(&images->lmb, images->ft_addr);
	return boot_relocate_fdt(&images->lmb, &images->ft_addr,
		&images->ft_len);
#else
	return 0;
#endif
}

static int boot_setup_fdt(bootm_headers_t *images)
{
	images->initrd_start = virt_to_phys((void *)images->initrd_start);
	images->initrd_end = virt_to_phys((void *)images->initrd_end);
	return image_setup_libfdt(images, images->ft_addr, images->ft_len,
		&images->lmb);
}

static void boot_prep_linux(bootm_headers_t *images)
{
	if (images->ft_len) {
		boot_reloc_fdt(images);
		boot_setup_fdt(images);
	} else {
#ifdef CONFIG_LOONGARCH_BOOT_CMDLINE_LEGACY
			linux_cmdline_legacy(images);
			// linux_cmdline_append(images);
			linux_cmdline_dump();
			linux_env_legacy(images);
#endif
	}
}

/*
 * 判断使用哪种传参方式
 * 0 代表旧的传参
 * 1 代表新的传参
 */
static int judge_boot_param_type(bootm_headers_t *images)
{
	char* kernel_type;
	kernel_type = image_get_name(images->legacy_hdr_os);

	if (!strncmp(kernel_type, "Linux-5", 7))
		return 0;
	else if (!strncmp(kernel_type, "Linux-4", 7))
		return 0;

	return 1;
}

static const char* boot_smbios_type2_board_name(void)
{
	struct udevice *dev;
	ofnode parent_node, node;

	const char* board_name = NULL;
	uclass_first_device(UCLASS_SYSINFO, &dev);
	if (dev) {
		parent_node = dev_read_subnode(dev, "smbios");
		if (!ofnode_valid(parent_node))
			return NULL;

		node = ofnode_find_subnode(parent_node, "baseboard");
		if (!ofnode_valid(node))
			return NULL;

		board_name = ofnode_read_string(node, "product");
	}
	return board_name;
}

static void boot_jump_linux(bootm_headers_t *images)
{
	typedef void __noreturn (*kernel_entry_t)(int, ulong, ulong, ulong);
	kernel_entry_t kernel = (kernel_entry_t)map_to_sysmem((void*)images->ep);
#if defined(CONFIG_LOONGSON_BOOT_FIXUP)
	void *fw_arg2 = NULL, *fw_arg3 = NULL;
	void *bootparam = NULL;
#endif

	debug("## Transferring control to Linux (at address %p) ...\n", kernel);

	bootstage_mark(BOOTSTAGE_ID_RUN_OS);

#if CONFIG_IS_ENABLED(BOOTSTAGE_FDT)
	bootstage_fdt_add_report();
#endif
#if CONFIG_IS_ENABLED(BOOTSTAGE_REPORT)
	bootstage_report();
#endif

#if defined(CONFIG_LOONGSON_BOOT_FIXUP)
	bootparam = build_boot_param();
	if (judge_boot_param_type(images)) {
		int i;
		const char* board_name;
		// 见于 龙芯CPU统一系统架构规范（LA架构嵌入式系列）.pdf 的 4.1 传参约定 一节
		fw_arg2 = (void*)(*(unsigned long long *)(bootparam + 8));
		board_name = boot_smbios_type2_board_name();

		// 新的传参规范里面无需把 command line 拆开成一个一个 直接字符串传进去就好
		// 下标 0 的元素必定为 NULL 无视即可
		linux_command_line = (char*)calloc(256, sizeof(char));
		sprintf(linux_command_line, "%s", linux_argv[1]);
		for (i = 2; i < linux_argc; ++i) {
			sprintf(linux_command_line, "%s %s", linux_command_line, linux_argv[i]);
		}
		// 把要匹配的板卡名字放到 command line 里面
		sprintf(linux_command_line, "%s bp_start=0x%.llx", linux_command_line, (unsigned long long)bootparam);
		if (board_name)
			sprintf(linux_command_line, "%s board_name=%s", linux_command_line, board_name);

		kernel(0, (ulong)linux_command_line, (ulong)fw_arg2, (ulong)fw_arg3);
	} else {
		void *fdt = NULL;
		fdt = env_get("fdt_addr");
		if (fdt) {
			fdt = (void *)simple_strtoul(fdt, NULL, 16);
			if (fdt_check_header(fdt)) {
				printf("Warning: invalid device tree. Used linux default dtb\n");
				fdt = NULL;
			}
		}

		fw_arg2 = bootparam;
		fw_arg3 = fdt;

		kernel(linux_argc, (ulong)linux_argv, (ulong)fw_arg2, (ulong)fw_arg3);
	}

#else
	if (images->ft_len) {
		kernel(-2, (ulong)images->ft_addr, 0, 0);
	} else {
		kernel(linux_argc, (ulong)linux_argv, (ulong)linux_env,
			linux_extra);
	}
#endif
}

int do_bootm_linux(int flag, int argc, char *const argv[],
		   bootm_headers_t *images)
{
	if (flag & BOOTM_STATE_OS_BD_T) // TODO
		return -1;

	/*
	 * Cmdline init has been moved to 'bootm prep' because it has to be
	 * done after relocation of ramdisk to always pass correct values
	 * for rd_start and rd_size to Linux kernel.
	 */
	if (flag & BOOTM_STATE_OS_CMDLINE)
		return 0;

	if (flag & BOOTM_STATE_OS_PREP) {
		boot_prep_linux(images);
		sync();
		return 0;
	}

	if (flag & (BOOTM_STATE_OS_GO | BOOTM_STATE_OS_FAKE_GO)) {
		boot_jump_linux(images);
		return 0;
	}

	/* does not return */
	return 1;
}
