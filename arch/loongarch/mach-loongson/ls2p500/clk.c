#include <config.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <mach/loongson.h>

DECLARE_GLOBAL_DATA_PTR;

void get_clocks(void)
{
	u32 refclk = REF_FREQ * 1000; //参考时钟固定为100MHz
	u32 ctrl = 0;
	unsigned int l1div_out, l1div_loopc, l1div_ref;
	unsigned int l2div_out;
	unsigned int mult, div;

	/* sys cpu  clk */
	ctrl = readl((void*)LS_SYS_PLL_L);
	l1div_out = (ctrl >> SYS_L1DIV_OUT_SHIFT) & SYS_L1DIV_OUT_MARK;
	l1div_loopc = (ctrl >> SYS_L1DIV_LOOPC_SHIFT) & SYS_L1DIV_LOOPC_MARK;
	l1div_ref = (ctrl >> SYS_L1DIV_REF_SHIFT) & SYS_L1DIV_REF_MARK;
	mult = l1div_loopc;
	div = l1div_ref * l1div_out;
	gd->cpu_clk = (unsigned long)((refclk * mult / div) * 1000);

	/*sys usb gmac apb clk*/
	ctrl = readl((void*)LS_SYS_PLL_H);
	l2div_out = (ctrl >> SYS_L2DIV_OUT_SOC_SHIFT) & SYS_L2DIV_OUT_SOC_MARK;
	// l1div_loopc = (ctrl >> SYS_L1DIV_LOOPC_SHIFT) & SYS_L1DIV_LOOPC_MARK;
	// l1div_ref = (ctrl >> SYS_L1DIV_REF_SHIFT) & SYS_L1DIV_REF_MARK;
	// mult = l1div_loopc;
	div = l1div_ref * l2div_out;
	gd->bus_clk = (unsigned long)((refclk * mult / div) * 1000);


	/* ddr net  clk */
	ctrl = readl((void*)LS_DDR_PLL_L);
	l1div_out = (ctrl >> DDR_L1DIV_OUT_SHIFT) & DDR_L1DIV_OUT_MARK;
	l1div_loopc = (ctrl >> DDR_L1DIV_LOOPC_SHIFT) & DDR_L1DIV_LOOPC_MARK;
	l1div_ref = (ctrl >> DDR_L1DIV_REF_SHIFT) & DDR_L1DIV_REF_MARK;
	ctrl = readl((void*)LS_DDR_PLL_H);
	mult = l1div_loopc;
	div = l1div_ref * l1div_out;
	gd->mem_clk = (unsigned long)((refclk * mult / div) * 1000);
}