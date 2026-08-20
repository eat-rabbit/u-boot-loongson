#include <stdlib.h>
#include "net.h"

static int m__net_gl_read(struct net_gl_desc* self, int proto,
		char* filepath, u64 size, u64* retsize, void* buf)
{
	int ret;

	// netretry yes 本身是做为双网口切换准备的，通常情况下没有问题
	// 因为操作者知道自已的网络设备上是否存在需要的文件或者能够检查网口情况
	// 然而在2K300上竟然有操作者不一定知道的文件需要存在
	// 就是 loongson_try_read_img_fdisk 中需要 fdisk.txt，这个过程是对操作者隐匿的
	// 好在目前只有2K300用 disk.img 模式，暂时需要这样规避
	// 对于其他所有使用 disk.img 的情况，务必、务必修改整体设计结构！
	// 禁止存在对操作者隐匿的条件
#ifdef CONFIG_LOONGSON_GENERAL_LOAD_FIX_FDISK
	env_set("netretry", "no");
#else
	env_set("netretry", "yes");
#endif
	env_set("ethrotate", "yes");

	copy_filename(net_boot_file_name, filepath,
		      sizeof(net_boot_file_name));
	net_server_ip = self->ip;

	ret = net_loop(proto);
	if (ret > 0)
	{
		*retsize = ret;
		ret = 0;
	}
	return ret;
}

struct net_gl_desc* net_gl_init(char* ip)
{
	struct net_gl_desc* desc = calloc(1, sizeof(*desc));
	if (desc == NULL)
		return NULL;

	desc->ip = string_to_ip(ip);
	if (desc->ip.s_addr == 0)
	{
		free(desc);
		return NULL;
	}

	desc->read = m__net_gl_read;
	return desc;
}

void net_gl_destroy(struct net_gl_desc* desc)
{
	if (desc != NULL)
		free(desc);
}

