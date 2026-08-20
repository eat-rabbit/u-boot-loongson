#ifndef __GENERAL_LOAD_DEBUG__
#define __GENERAL_LOAD_DEBUG__

#ifdef DBG
#define GL_PRINTF(fmt, args...) printf("[GeneralLoad]"#fmt"\n", ##args)
#else
#define GL_PRINTF(fmt, args...)
#endif

#endif
