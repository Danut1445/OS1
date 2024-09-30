// SPDX-License-Identifier: BSD-3-Clause

#include <fcntl.h>
#include <internal/syscall.h>
#include <stdarg.h>
#include <errno.h>

int open(const char *filename, int flags, ...)
{
	int fisier = syscall(__NR_open, filename, flags);

	if (fisier < 0) {
		errno = -fisier;
		return -1;
	}

	return fisier;
}
