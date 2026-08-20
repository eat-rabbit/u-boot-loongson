#ifndef __CMD_GENERAL_LOAD_ARG_H__
#define __CMD_GENERAL_LOAD_ARG_H__

#include "general_load.h"

int cmd_gl_parse(char** arg, int n,
		gl_target_t* src, gl_target_t* dest, enum gl_extra_e* extra);

#endif
