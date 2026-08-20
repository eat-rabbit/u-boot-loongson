#include <stdio.h>
#include <u-boot/crc.h>
#include "flash_secure.h"

bool uboot_secure(void* buf, u64 size)
{
	printf("uboot secure check\n");
	return true;
}

