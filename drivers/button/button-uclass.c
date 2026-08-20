// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 Philippe Reynes <philippe.reynes@softathome.com>
 *
 * Based on led-uclass.c
 */

#define LOG_CATEGORY UCLASS_BUTTON

#include <common.h>
#include <button.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <input.h>
#include <stdio_dev.h>

int button_get_by_label(const char *label, struct udevice **devp)
{
	struct udevice *dev;
	struct uclass *uc;

	uclass_id_foreach_dev(UCLASS_BUTTON, dev, uc) {
		struct button_uc_plat *uc_plat = dev_get_uclass_plat(dev);

		/* Ignore the top-level button node */
		if (uc_plat->label && !strcmp(label, uc_plat->label))
			return uclass_get_device_tail(dev, 0, devp);
	}

	return -ENODEV;
}

enum button_state_t button_get_state(struct udevice *dev)
{
	struct button_ops *ops = button_get_ops(dev);

	if (!ops->get_state)
		return -ENOSYS;

	return ops->get_state(dev);
}

#ifdef CONFIG_BUTTON_GPIO
static struct input_config button_input;

static int button_get_code(struct udevice *dev)
{
	struct button_ops *ops = button_get_ops(dev);

	if (!ops->get_state)
		return -ENOSYS;

	return ops->get_code(dev);
}

static int button_read_keys(struct input_config *input)
{
	struct udevice *dev;
	struct uclass *uc;
	uclass_id_foreach_dev(UCLASS_BUTTON, dev, uc) {
		struct button_uc_plat *uc_plat = dev_get_uclass_plat(dev);

		// all gpio button probe
		if (uc_plat->label)
			if (button_get_state(dev) == BUTTON_ON) {
				int key = 0;
				key = button_get_code(dev);
				input_send_keycodes(&button_input, &key, 1);
			}
	}

	return 1;
}

static int button_getc(struct stdio_dev *dev)
{
	return input_getc(&button_input);
}

static int button_tstc(struct stdio_dev *dev)
{
	return input_tstc(&button_input);
}

static int button_init(struct stdio_dev *dev)
{
	struct udevice *udev;
	struct udevice *udevp;
	struct uclass *uc;
	uclass_id_foreach_dev(UCLASS_BUTTON, udev, uc) {
		// all gpio button probe
		uclass_get_device_tail(udev, 0, &udevp);
	}
	input_set_delays(&button_input, 250, 250);
	return 0;
}

int drv_button_input_init(void)
{
	int error;
	struct stdio_dev dev = {
		.name	= "gpiobtn",
		.flags	= DEV_FLAGS_INPUT,
		.start	= button_init,
		.getc	= button_getc,
		.tstc	= button_tstc,
	};

	error = input_init(&button_input, 0);
	if (error) {
		debug("%s: init input failed!!\n", __func__);
		return -1;
	}
	input_add_tables(&button_input, false);
	button_input.read_keys = button_read_keys;

	error = input_stdio_register(&dev);
	if (error)
		return error;

	return 0;
}
#endif

UCLASS_DRIVER(button) = {
	.id		= UCLASS_BUTTON,
	.name		= "button",
	.per_device_plat_auto	= sizeof(struct button_uc_plat),
};

