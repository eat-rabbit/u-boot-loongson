#ifndef __LOONGSON_BOOT_SYSPART_MANAGER_H__
#define __LOONGSON_BOOT_SYSPART_MANAGER_H__

#define SYSPART_FIRST "1"
#define SYSPART_FIRST_UBOOT_ENV_NAME "syspart"
#define SYSPART_SECOND "4"
#define SYSPART_SECOND_UBOOT_ENV_NAME "syspart_last"

#define SYSPART_USER_CH_UBOOT_ENV_NAME "syspart_ch"
#define SYSPART_USER_CH_DISABLE "0"
#define SYSPART_USER_CH_ENABLE "1"

int switch_syspart(void);
int setup_cur_syspart(char* syspart);
int detect_user_change_syspart(void);

#endif
