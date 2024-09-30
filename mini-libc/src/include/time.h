#ifndef _TIME_H
#define _TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <internal/syscall.h>
#include <internal/types.h>

struct timespec
{
  long unsigned tm_sec;
  long unsigned tm_nsec;
};

int nanosleep(const struct timespec *t1, struct timespec *t2);

#ifdef __cplusplus
}
#endif

#endif
