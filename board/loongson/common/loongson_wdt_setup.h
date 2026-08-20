#ifndef __LOONGSON_WDT_SETUP_H__
#define __LOONGSON_WDT_SETUP_H__

#define LS_WDT_NODE_NAME "watchdog_d"
#define LS_WDT_DEFAULT_TIMEOUT_MS 34000

int ls_wdt_start(int ms);
int ls_wdt_stop(void);
int ls_wdt_restart(void);

#endif
