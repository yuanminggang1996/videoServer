#ifndef __LOG_H_
#define __LOG_H_

#include <stdio.h>
#include <string.h>
#define DEBUG

#ifdef DEBUG
#define DBGLOG(fmt, args...) fprintf(stderr, "[%s %d]"fmt, strstr(__FILE__, "src"), __LINE__, ##args)
#else
#define DBGLOG(fmt, args...)
#endif

#endif //__LOG_H_