// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Jianhui Wang <wangjianhui@loongson.cn>
 */

#include <common.h>
#include <dm.h>
#include <sound.h>
#include <asm/gpio.h>
#include <asm/io.h>

struct gpio_beeper_priv {
	struct gpio_desc gpio;
};

int gpio_beeper_start_beep(struct udevice *dev, int frequency_hz)
{
	struct gpio_beeper_priv *priv = dev_get_priv(dev);
	return dm_gpio_set_value(&priv->gpio, 1);
}

int gpio_beeper_stop_beep(struct udevice *dev)
{
	struct gpio_beeper_priv *priv = dev_get_priv(dev);
	return dm_gpio_set_value(&priv->gpio, 0);
}

static const struct sound_ops gpio_beeper_ops = {
	.start_beep	= gpio_beeper_start_beep,
	.stop_beep	= gpio_beeper_stop_beep,
};

static int gpio_beeper_of_to_plat(struct udevice *dev)
{
	struct gpio_beeper_priv *priv = dev_get_priv(dev);
	int ret;

	ret = gpio_request_by_name(dev, "gpios", 0, &priv->gpio,
				   GPIOD_IS_OUT);
	if (ret) {
		debug("%s: Warning: cannot get GPIO: ret=%d\n",
		      __func__, ret);
		return ret;
	}

	return 0;
}

static int gpio_beeper_probe(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id gpio_beeper_ids[] = {
	{ .compatible = "gpio-beeper" },
	{ }
};

U_BOOT_DRIVER(gpio_beeper) = {
	.name		= "gpio-beeper",
	.id		= UCLASS_SOUND,
	.of_match	= gpio_beeper_ids,
	.ops		= &gpio_beeper_ops,
	.of_to_plat	= gpio_beeper_of_to_plat,
	.probe		= gpio_beeper_probe,
	.priv_auto	= sizeof(struct gpio_beeper_priv),
};