#include <common.h>
#include <init.h>
#include <spl.h>
#include <asm/io.h>
#include <asm/addrspace.h>
#include <mach/loongson.h>

void spl_mach_init(void)
{
	arch_cpu_init();
}

static void setup_gpio_value(unsigned int gpio, int value)
{
	unsigned char val;
	if (gpio > LS_GPIO_MAX) {
		printf("Error: setup_gpio_value: gpio(%d) not exist!\n", gpio);
		return;
	}
	val = value ? 1 : 0;
	readb(LS_GPIO_B_DIR_BASE + gpio) = 0x0;
	readb(LS_GPIO_B_OUT_BASE + gpio) = val;
}

void spl_board_init_late(void)
{
	if (!strcmp(CONFIG_DEFAULT_DEVICE_TREE, "ls2k300_pai"))
		setup_gpio_value(75, 1);
}

void spl_mach_init_late(void)
{
	unsigned long unlock_base = LOCK_CACHE_BASE;

	debug("spl unlocked scache.\n");
    // unlock scache
	// the scache locked in lowlevel_init.S is used by stack for early stage.
	// now our stack sp is in sdram, so unlock the scache.
	writeq(0x0, LS_SCACHE_LOCK_WIN0_BASE);
	writeq(0x0, LS_SCACHE_LOCK_WIN0_MASK);

	/* flush scache using hit-invalidate */
	for (int i = 0; i < LOCK_CACHE_SIZE; i += 0x40) {
		asm(
			"cacop 0x13, %0, 0\n\t"
			:
			: "r"(unlock_base)
		);
		unlock_base += 0x40;
	}
}
