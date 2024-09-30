// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <internal/syscall.h>
#include <errno.h>

int truncate(const char *path, off_t length)
{
	int truncate = syscall(__NR_truncate, path, length);

	if (truncate < 0) {
		errno = -truncate;
		return -1;
	}

	return truncate;
}
