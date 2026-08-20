/*
* Copyright (C) 2017 Loongson Technology Corporation Limited
*
* Author: Juxin Gao <gaojuxin@loongson.cn>
* License terms: GNU General Public License (GPL)
*/

/*
	e.g.
	pwm2: pwm2@0x1ff5c020 {
		compatible = "loongson,ls2k-pwm";
		reg = <0 0x1ff5c020 0 0x10>;
		interrupt-parent = <&extioiic>;
		interrupts = <42>;
		clocks = <&apb_clk>;
		clock-names = "pwm-clk";
		status = "disable";
	};
 */

#include <clk.h>
#include <common.h>
#include <div64.h>
#include <dm.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <pwm.h>
#include <linux/delay.h>

/* counter offest */
#define LOW_BUFFER  0x004
#define FULL_BUFFER 0x008
#define CTRL		0x00c

/* CTRL counter each bit */
#define CTRL_EN		BIT(0)
#define CTRL_OE		BIT(3)
#define CTRL_SINGLE	BIT(4)
#define CTRL_INTE	BIT(5)
#define CTRL_INT	BIT(6)
#define CTRL_RST	BIT(7)
#define CTRL_CAPTE	BIT(8)
#define CTRL_INVERT	BIT(9)
#define CTRL_DZONE	BIT(10)

#define to_ls_pwm_chip(_chip)		container_of(_chip, struct ls_pwm_chip, chip)
#define NS_IN_HZ (1000000000UL)
#define CPU_FRQ_PWM (50000000UL)

#define SPI_MAX_ID 5

enum pwm_polarity {
	PWM_POLARITY_NORMAL,
	PWM_POLARITY_INVERSED,
};

struct ls_pwm_chip{
	void __iomem		*mmio_base;
	struct clk pclk;
	u32 clock_frequency;
	/* following registers used for suspend/resume */
	u32	ctrl_reg;
	u32	low_buffer_reg;
	u32	full_buffer_reg;
	// u32	clock_frequency;
};

static int ls2k_pwm_set_invert(struct udevice *dev, uint channel,
					bool polarity)
{
	struct ls_pwm_chip *ls_pwm = dev_get_priv(dev);
	u16 val;

	val = readl(ls_pwm->mmio_base + CTRL);
	if (polarity)
		val |= CTRL_INVERT;
	else
		val &= ~CTRL_INVERT;

	writel(val, ls_pwm->mmio_base + CTRL);
	return 0;
}

static int ls_pwm_disable(struct udevice *dev, uint channel)
{
	struct ls_pwm_chip *ls_pwm = dev_get_priv(dev);
	u32 ret;

	/*
	* when disbale we let output is 0 duty_cycle state
	* so normal polarity output high voltage
	* and inversed polarity output low voltage
	* just because led pwm and pwm backlight
	*
	* about ls pwm, low buffer just need 1 is 0 duty cycle
	* and if init so that period just 0
	* we need put a num which bigger than 1 to full buufer
	* so it just will run as 0 duty cycle
	*/
	writel(1, ls_pwm->mmio_base + LOW_BUFFER);
	if (!(ls_pwm->full_buffer_reg))
		writel(10000, ls_pwm->mmio_base + FULL_BUFFER);

	/*
	* setup 0 duty cycle
	* bu ls pwm have low_level full_pulse which can't contorl
	* low_level from LOW_BUFFER, full_pulse from FULL_BUFFER
	* and pwm output state depend on low_level and full_pulse
	* no BUFFER
	* so we need reset low_level and full_pulse to let 0 duty cycle setup token effect
	*/
	ret = readl(ls_pwm->mmio_base + CTRL);
	ret |= CTRL_RST;
	writel(ret, ls_pwm->mmio_base + CTRL);
	ret &= ~CTRL_RST;
	writel(ret, ls_pwm->mmio_base + CTRL);

	/*
	* stop count
	* but dont setup CTRL_OE
	* it will output low voltage under normal polarity or inversed polarity
	*/
	ret = readl(ls_pwm->mmio_base + CTRL);
	ret &= ~CTRL_EN;
	writel(ret, ls_pwm->mmio_base + CTRL);

	return 0;
}

static int ls_pwm_enable(struct udevice *dev, uint channel)
{
	struct ls_pwm_chip *ls_pwm = dev_get_priv(dev);
	int ret;

	writel(ls_pwm->low_buffer_reg, ls_pwm->mmio_base + LOW_BUFFER);
	writel(ls_pwm->full_buffer_reg, ls_pwm->mmio_base + FULL_BUFFER);

	ret = readl(ls_pwm->mmio_base + CTRL);
	ret |= CTRL_EN;
	writel(ret, ls_pwm->mmio_base + CTRL);
	return 0;
}

static int ls2k_pwm_set_enable(struct udevice *dev, uint channel, bool enable)
{
	if (enable)
		return ls_pwm_enable(dev, channel);
	else
		return ls_pwm_disable(dev, channel);
}

#define NSEC_PER_SEC 1000000000

static int ls2k_pwm_set_config(struct udevice *dev, uint channel,
					uint period_ns, uint duty_ns)
{
	struct ls_pwm_chip *ls_pwm = dev_get_priv(dev);
	unsigned int period, duty;
	unsigned long long val0,val1;

	if (period_ns > NS_IN_HZ || duty_ns > NS_IN_HZ)
		return -ERANGE;

	// must unsigned long long other will calu error
	val0 = (unsigned long long)ls_pwm->clock_frequency *  (unsigned long long)period_ns;
	do_div(val0, NSEC_PER_SEC);
	if (val0 < 1)
		val0 = 1;
	period = val0;

	val1 = (unsigned long long)ls_pwm->clock_frequency *  (unsigned long long)duty_ns;
	do_div(val1, NSEC_PER_SEC);
	if (val1 < 1)
		val1 = 1;
	duty = val1;

	writel(duty,ls_pwm->mmio_base + LOW_BUFFER);
	writel(period,ls_pwm->mmio_base + FULL_BUFFER);
	ls_pwm->low_buffer_reg = duty;
	ls_pwm->full_buffer_reg = period;

	/*
	* delay 1 of period_ns to ensure pwm setup enable
	* in pwm-led, if not delay, when set 0 duty_cycle and set pwm disable right now
	* it maybe setup error, such as max-brightness is 1024
	* cur brightness is 100, and set brightness 1024, led will off, this is not right.
	* just because when setup 1024, will setup 0 duty_cycle and disable pwm
	* but loongson pwm set CTRL_EN 0 will keep last output
	* so if not delay, last output more will be high(led off), because 100 brightness more time is high
	*/
	udelay(period_ns);

	return 0;
}


static int ls2k_pwm_probe(struct udevice *dev)
{
	struct ls_pwm_chip *priv = dev_get_priv(dev);
	int ret;

	priv->mmio_base = dev_read_addr_ptr(dev);
	if (!priv->mmio_base)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &priv->pclk);
	if (ret)
		return ret;

	/* clocks aren't ref-counted so just enabled them once here */
	ret = clk_enable(&priv->pclk);
	if (ret)
		return ret;

	priv->clock_frequency = clk_get_rate(&priv->pclk);

	return ret;
}

static const struct pwm_ops ls2k_pwm_ops = {
	.set_config = ls2k_pwm_set_config,
	.set_enable = ls2k_pwm_set_enable,
	.set_invert = ls2k_pwm_set_invert,
};

static const struct udevice_id ls2k_pwm_of_match[] = {
	{ .compatible = "loongson,ls2k-pwm" },
	{ }
};

U_BOOT_DRIVER(ls2k_pwm) = {
	.name = "ls2k_pwm",
	.id = UCLASS_PWM,
	.of_match = ls2k_pwm_of_match,
	.probe = ls2k_pwm_probe,
	.priv_auto = sizeof(struct ls_pwm_chip),
	.ops = &ls2k_pwm_ops,
};

