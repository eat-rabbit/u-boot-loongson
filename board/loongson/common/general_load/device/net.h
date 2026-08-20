#ifndef __GENERAL_LOAD_NET_H__
#define __GENERAL_LOAD_NET_H__

#include <net.h>

#define net_gl_read(d, p, f, s, r, b) d->read(d, p, f, s, r, b)

struct net_gl_desc {
	struct in_addr ip;
	int (*read)(struct net_gl_desc* self, int proto,
			char* filepath, u64 size, u64* retsize, void* buf);
};

struct net_gl_desc* net_gl_init(char* ip);
void net_gl_destroy(struct net_gl_desc* desc);

#endif
