#ifndef __LOG_H_
#define __LOG_H_

#include <stdio.h>
#define DEBUG

#ifdef DEBUG
#define DBGLOG(fmt, args...) fprintf(stderr, "[%s %d]"fmt, __FILE__, __LINE__, ##args)
#else
#define DBGLOG(fmt, args...)
#endif

#endif //__LOG_H_