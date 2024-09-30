// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>

char *strcpy(char *destination, const char *source)
{
	int i = 0;
	while(source[i]) {
		destination[i] = source[i];
		i++;
	}
	destination[i] = '\0';
	return destination;
}

char *strncpy(char *destination, const char *source, size_t len)
{
	for (size_t i = 0; i < len; i++)
		destination[i] = source[i];
	return destination;
}

char *strcat(char *destination, const char *source)
{
	size_t len = strlen(destination);
	size_t i = 0;
	while(source[i]) {
		destination[i + len] = source[i];
		i++;
	}
	destination[i + len] = '\0';
	return destination;
}

char *strncat(char *destination, const char *source, size_t len)
{
	size_t len2 = strlen(destination);
	for (size_t i = 0; i < len; i++) {
		destination[i + len2] = source[i];
	}
	destination[len2 + len] = '\0';
	return destination;
}

int strcmp(const char *str1, const char *str2)
{
	int i = 0, cmp = 0;
	while (str1[i] && str2[i]) {
		cmp = str1[i] - str2[i];
		if (cmp)
			return cmp;
		i++;
	}
	if (str1[i] && !str2[i])
		return 1;
	if (!str1[i] && str2[i])
		return -1;
	return 0;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
	int cmp = 0;
	for (size_t i = 0; i < len; i++) {
		if (str1[i] && !str2[i])
			return 1;
		if (!str1[i] && str2[i])
			return -1;
		cmp = str1[i] - str2[i];
		if (cmp)
			return cmp;
	}
	return 0;
}

size_t strlen(const char *str)
{
	size_t i = 0;

	for (; *str != '\0'; str++, i++)
		;

	return i;
}

char *strchr(const char *str, int c)
{
	int i = 0;
	while(str[i]) {
		if (str[i] == c)
			return str + i;
		i++;
	}
	return NULL;
}

char *strrchr(const char *str, int c)
{
	size_t i = strlen(str);
	while(i) {
		if (str[i] == c)
			return str + i;
		i--;
	}
	return NULL;
}

char *strstr(const char *haystack, const char *needle)
{
	for (size_t i = 0; i < strlen(haystack) - strlen(needle); i++) {
		if (haystack[i] == needle[0]) {
			for (size_t j = 0; j < strlen(needle); j++) {
				if (needle[j] != haystack[i + j])
					break;
				if (j == strlen(needle) - 1)
					return haystack + i;
			}
		}
	}
	return NULL;
}

char *strrstr(const char *haystack, const char *needle)
{
	for (size_t i = strlen(haystack) - strlen(needle) + 1; i > 0; i--) {
		if (haystack[i - 1] == needle[0]) {
			for (size_t j = 0; j < strlen(needle); j++) {
				if (needle[j] != haystack[i + j - 1])
					break;
				if (j == strlen(needle) - 1)
					return haystack + i - 1;
			}
		}
	}
	return NULL;
}

void *memcpy(void *destination, const void *source, size_t num)
{
	for (size_t i = 0; i < num; i++) {
		((char *)destination)[i] = ((char *)source)[i];
	}
	return destination;
}

void *memmove(void *destination, const void *source, size_t num)
{
	for (size_t i = 0; i < num; i++) {
		((char *)destination)[i] = ((char *)source)[i];
	}
	return destination;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	int cmp = 0;
	for (size_t i = 0; i < num; i++) {
		cmp = ((char *)ptr1)[i] - ((char *)ptr2)[i];
		if (cmp)
			return cmp;
	}
	return 0;
}

void *memset(void *source, int value, size_t num)
{
	for (size_t i = 0; i < num; i++) {
		((char *)source)[i] = value;
	}
	return source;
}
