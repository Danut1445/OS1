// SPDX-License-Identifier: BSD-3-Clause

#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <internal/essentials.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

void *malloc(size_t size)
{
	void *start = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	mem_list_add(start, size);
	return start;
}

void *calloc(size_t nmemb, size_t size)
{
	void *start = malloc(size * nmemb);
	memset(start, 0, size * nmemb);
	return start;
}

void free(void *ptr)
{
	struct mem_list *item = mem_list_find(ptr);
	munmap(ptr, item->len);
	mem_list_del(ptr);
}

void *realloc(void *ptr, size_t size)
{
	void *start = malloc(size);
	struct mem_list *item = mem_list_find(ptr);
	if (size > item->len)
		memcpy(start, ptr, item->len);
	else
		memcpy(start, ptr, size);
	free(ptr);
	return start;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
	return realloc(ptr, nmemb * size);
}
