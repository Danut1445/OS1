#include <internal/syscall.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

int nanosleep(const struct timespec *t1, struct timespec *t2)
{
	long int ok = syscall(__NR_nanosleep, t1, t2);
	if (ok < 0) {
		errno = -ok;
		return -1;
	}
	return 0;
}
