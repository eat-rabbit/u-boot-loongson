#ifndef __GENERAL_LOAD_H__
#define __GENERAL_LOAD_H__

#include <net.h>
#include <fs.h>
#include "gl_target.h"

enum gl_extra_e {
	GL_EXTRA_NONE,
	GL_EXTRA_DECOMPRESS = 1,
	GL_EXTRA_UBOOTSECURE = 2,
};

// load-from <src-target> burn-to <dest-target>
// burn-to decoration: <decompress/securecheck>
// target decoration:
// 	device <blk/net>,
//	format <fs/netproto>
//	symbol <filename>
// target: <symbol> in <device> with <format>
//
// load-from <src> burn-to <dest> and <extra>
// e.g.
// load {/update/uImage in usb0:1 with fs-ext4} to {/boot/uImage in mmc0:1 with fs-ext4}
// load {/a.gz in usb0:1 with fs-ext4} to {/root/a in mmc0:1 with fs-ext4} and {decompression}
// load {disk.img.gz in net:192.168.1.2 with netproto-tftp} to {mmc0} and {decompression}
// load {uboot in net:192.168.1.2 with netproto-tftp} to {sf:0} and {securecheck}
// load {mmc0} to {scsi0}

int general_load(gl_target_t* src, gl_target_t* dest, enum gl_extra_e extra);

#endif

