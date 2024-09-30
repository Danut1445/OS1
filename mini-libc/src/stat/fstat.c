// SPDX-License-Identifier: BSD-3-Clause

#include <sys/stat.h>
#include <internal/syscall.h>
#include <errno.h>

int fstat(int fd, struct stat *st)
{
	int fstat = syscall(__NR_fstat, fd, st);

	if (fstat < 0) {
		errno = -fstat;
		return -1;
	}

	return fstat;
}
