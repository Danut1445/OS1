// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <internal/syscall.h>
#include <errno.h>

int ftruncate(int fd, off_t length)
{
	int truncate = syscall(__NR_ftruncate, fd, length);

	if (truncate < 0) {
		errno = -truncate;
		return -1;
	}

	return truncate;
}
