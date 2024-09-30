// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <internal/syscall.h>
#include <errno.h>

off_t lseek(int fd, off_t offset, int whence)
{
	int seek = syscall(__NR_lseek, fd, offset, whence);

	if (seek < 0) {
		errno = -seek;
		return -1;
	}

	return seek;
}
