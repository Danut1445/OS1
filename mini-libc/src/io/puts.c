#include <internal/io.h>
#include <internal/syscall.h>
#include <internal/types.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

int puts(const char *str)
{
	char final[256];
	strcpy(final, str);
	char* back = "\n";
	strcat(final, back);
	int len = strlen(final);
	return write(1, final, len);
}
