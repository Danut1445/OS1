// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <internal/syscall.h>
#include <stdarg.h>
#include <errno.h>

int close(int fd)
{
	int fisier = syscall(__NR_close, fd);

	if (fisier < 0) {
		errno = -fisier;
		return -1;
	}

	return fisier;
}
