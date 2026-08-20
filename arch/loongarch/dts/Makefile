# SPDX-License-Identifier: GPL-2.0+

dtb-$(CONFIG_SOC_LS2K500) += ls2k500_mini_dp.dtb
dtb-$(CONFIG_SOC_LS2K1000) += ls2k1000_dp.dtb

include $(srctree)/scripts/Makefile.dts

targets += $(dtb-y)

# Add any required device tree compiler flags here
DTC_FLAGS +=

PHONY += dtbs
dtbs: $(addprefix $(obj)/, $(dtb-y))
	@:

clean-files := *.dtb
