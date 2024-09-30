// SPDX-License-Identifier: BSD-3-Clause
#include "osmem.h"
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include "../utils/block_meta.h"

#define ALIGN(size) (((size) + 7) & ~7)
#define HEADER_SIZE (ALIGN(sizeof(struct block_meta)))
#define MMAP_THRESHOLD		(128 * 1024)

struct block_meta_list {
	struct block_meta *head;
	int size;
};

struct block_meta_list heap_list = {NULL, 0};
struct block_meta_list mmpa_list = {NULL, 0};

struct block_meta *find_best(size_t block_size)
{
	struct block_meta *curr = heap_list.head, *free_block = NULL;
	size_t max;

	while (curr->next) {
		if (curr->status == STATUS_FREE && curr->size - block_size + HEADER_SIZE < max && free_block) {
			free_block = curr;
			max = curr->size - block_size + HEADER_SIZE;
		}
		if (!free_block && curr->status == STATUS_FREE && curr->size >= block_size - HEADER_SIZE) {
			free_block = curr;
			max = curr->size - block_size + HEADER_SIZE;
		}
		curr = curr->next;
	}
	if (free_block)
		return free_block;
	if (curr->status == STATUS_FREE)
		return curr;
	return NULL;
}

void coalesce_blocks(void)
{
	struct block_meta *curr = heap_list.head, *aux;

	while (curr->next) {
		if (curr->status == STATUS_FREE && curr->next->status == STATUS_FREE) {
			aux = curr->next;
			curr->next = aux->next;
			if (curr->next)
				curr->next->prev = curr;
			curr->size = curr->size + HEADER_SIZE + aux->size;
			heap_list.size--;
		} else {
			curr = curr->next;
		}
	}
}

void *os_malloc(size_t size)
{
	if (!size)
		return NULL;
	printf("%d\n", size);
	void *alloced = NULL;
	struct block_meta *block = heap_list.head, *free_block;
	size_t block_size = ALIGN(size + HEADER_SIZE);

	if (block_size >= MMAP_THRESHOLD) {
		alloced = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (!mmpa_list.head) {
			mmpa_list.head = (struct block_meta *) alloced;
			mmpa_list.size = 1;
			block = mmpa_list.head;
			block->next = NULL;
			block->prev = NULL;
			block->size = block_size - HEADER_SIZE;
			block->status = STATUS_MAPPED;
		} else {
			block = mmpa_list.head;
			while (block->next)
				block = block->next;
			block->next = (struct block_meta *) alloced;
			block->next->prev = block;
			block->next->next = NULL;
			block->next->size = block_size - HEADER_SIZE;
			block->next->status = STATUS_MAPPED;
			heap_list.size++;
		}
		return alloced + HEADER_SIZE;
	}
	if (!heap_list.size) {
		alloced = sbrk(128*1024);
		if (128*1024 < block_size)
			sbrk(128*1024 - block_size);
		heap_list.head = (struct block_meta *) alloced;
		heap_list.size = 1;
		block = heap_list.head;
		block->size = block_size - HEADER_SIZE;
		if (block_size + HEADER_SIZE + 8 < 128 * 1024) {
			block->next = (struct block_meta *)(alloced + block_size);
			heap_list.size++;
			block->next->next = NULL;
			block->next->prev = block;
			block->next->size = MMAP_THRESHOLD - block_size - HEADER_SIZE;
			block->next->status = STATUS_FREE;
		} else {
			block->next = NULL;
		}
		block->prev = NULL;
		block->status = STATUS_ALLOC;
		return alloced + HEADER_SIZE;
	}
	coalesce_blocks();
	free_block = find_best(block_size);
	if (!free_block) {
		alloced = sbrk(block_size);
		while (block->next)
			block = block->next;
		block->next = (struct block_meta *) alloced;
		block->next->prev = block;
		block->next->next = NULL;
		block->next->size = block_size - HEADER_SIZE;
		block->next->status = STATUS_ALLOC;
		heap_list.size++;
		return alloced + HEADER_SIZE;
	}
	if (free_block->size > block_size - HEADER_SIZE) {
		if (free_block->size - block_size + HEADER_SIZE >= HEADER_SIZE + 8) {
			alloced = free_block;
			block = alloced + block_size;
			block->next = free_block->next;
			block->prev = free_block;
			block->size = free_block->size - block_size;
			block->status = STATUS_FREE;
			free_block->next = block;
			if (block->next)
				block->next->prev = block;
			free_block->size = block_size - HEADER_SIZE;
			heap_list.size++;
		}
		free_block->status = STATUS_ALLOC;
		return free_block + 1;
	}
	sbrk(block_size - free_block->size - HEADER_SIZE);
	free_block->size = block_size - HEADER_SIZE;
	free_block->status = STATUS_ALLOC;
	return free_block + 1;
}

void os_free(void *ptr)
{
	struct block_meta *curr;
	void *cautat;

	curr = mmpa_list.head;
	while (curr) {
		cautat = curr;
		cautat += HEADER_SIZE;
		if (cautat == ptr) {
			if (!curr->prev) {
				mmpa_list.head = curr->next;
				if (curr->next)
					curr->next->prev = NULL;
			} else {
				curr->prev->next = curr->next;
				if (curr->next)
					curr->next->prev = curr->prev;
			}
			mmpa_list.size--;
			munmap(curr, curr->size + HEADER_SIZE);
			return;
		}
		curr = curr->next;
	}
	curr = heap_list.head;
	while (curr) {
		cautat = curr;
		cautat += HEADER_SIZE;
		if (cautat == ptr) {
			curr->status = STATUS_FREE;
			return;
		}
		curr = curr->next;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (!size || !nmemb)
		return NULL;
	void *alloced = NULL;
	struct block_meta *block = heap_list.head, *free_block;
	size_t block_size = ALIGN(size * nmemb + HEADER_SIZE);

	if (block_size >= 4096) {
		alloced = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (!mmpa_list.size) {
			mmpa_list.head = (struct block_meta *) alloced;
			mmpa_list.size = 1;
			block = mmpa_list.head;
			block->next = NULL;
			block->prev = NULL;
			block->size = block_size - HEADER_SIZE;
			block->status = STATUS_MAPPED;
		} else {
			block = mmpa_list.head;
			while (block->next)
				block = block->next;
			block->next = (struct block_meta *) alloced;
			block->next->prev = block;
			block->next->next = NULL;
			block->next->size = block_size - HEADER_SIZE;
			block->next->status = STATUS_MAPPED;
			heap_list.size++;
		}
		memset(alloced + HEADER_SIZE, 0, block_size - HEADER_SIZE);
		return alloced + HEADER_SIZE;
	}
	if (!heap_list.size) {
		alloced = sbrk(128*1024);
		if (128*1024 < block_size)
			sbrk(128*1024 - block_size);
		heap_list.head = (struct block_meta *) alloced;
		heap_list.size = 1;
		block = heap_list.head;
		block->size = block_size - HEADER_SIZE;
		if (block_size + HEADER_SIZE + 8 < 128 * 1024) {
			block->next = (struct block_meta *)(alloced + block_size);
			heap_list.size++;
			block->next->next = NULL;
			block->next->prev = block;
			block->next->size = MMAP_THRESHOLD - block_size - HEADER_SIZE;
			block->next->status = STATUS_FREE;
		} else {
			block->next = NULL;
		}
		block->prev = NULL;
		block->status = STATUS_ALLOC;
		memset(alloced + HEADER_SIZE, 0, block_size - HEADER_SIZE);
		return alloced + HEADER_SIZE;
	}
	coalesce_blocks();
	free_block = find_best(block_size);
	if (!free_block) {
		alloced = sbrk(block_size);
		while (block->next)
			block = block->next;
		block->next = (struct block_meta *) alloced;
		block->next->prev = block;
		block->next->next = NULL;
		block->next->size = block_size - HEADER_SIZE;
		block->next->status = STATUS_ALLOC;
		heap_list.size++;
		memset(alloced + HEADER_SIZE, 0, block_size - HEADER_SIZE);
		return alloced + HEADER_SIZE;
	}
	if (free_block->size > block_size - HEADER_SIZE) {
		if (free_block->size - block_size + HEADER_SIZE >= HEADER_SIZE + 8) {
			alloced = free_block;
			block = alloced + block_size;
			block->next = free_block->next;
			block->prev = free_block;
			block->size = free_block->size - block_size;
			block->status = STATUS_FREE;
			free_block->next = block;
			if (block->next)
				block->next->prev = block;
			free_block->size = block_size - HEADER_SIZE;
			heap_list.size++;
		}
		free_block->status = STATUS_ALLOC;
		memset(free_block + 1, 0, block_size - HEADER_SIZE);
		return free_block + 1;
	}
	sbrk(block_size - free_block->size - HEADER_SIZE);
	free_block->size = block_size - HEADER_SIZE;
	free_block->status = STATUS_ALLOC;
	memset(free_block + 1, 0, block_size - HEADER_SIZE);
	return free_block + 1;
}

void *os_realloc(void *ptr, size_t size)
{
	if (!ptr)
		return os_malloc(size);
	if (!size) {
		os_free(ptr);
		return NULL;
	}
	size_t block_size = ALIGN(size + HEADER_SIZE);
	void *alloced;
	struct block_meta *curr, *block;

	curr = mmpa_list.head;
	while (curr) {
		if (curr + 1 == ptr)
			break;
		curr = curr->next;
	}
	if (curr) {
		if (block_size < MMAP_THRESHOLD) {
			alloced = os_malloc(size);
			if (curr->size > size)
				memcpy(alloced, curr + 1, size);
			else
				memcpy(alloced, curr + 1, curr->size);
		} else {
			alloced = mmap(NULL, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			block = mmpa_list.head;
			while (block->next)
				block = block->next;
			block->next = (struct block_meta *) alloced;
			block->next->prev = block;
			block->next->next = NULL;
			block->next->size = block_size - HEADER_SIZE;
			block->next->status = STATUS_MAPPED;
			if (curr->size > size)
				memcpy(alloced + HEADER_SIZE, curr + 1, size);
			else
				memcpy(alloced + HEADER_SIZE, curr + 1, curr->size);
		}
		mmpa_list.size--;
		if (mmpa_list.head == curr)
			mmpa_list.head = curr->next;
		else
			curr->prev->next = curr->next;
		if (curr->next)
			curr->next->prev = curr->prev;
		munmap(curr, curr->size + HEADER_SIZE);
		if (block_size >= MMAP_THRESHOLD)
			return alloced + HEADER_SIZE;
		else
			return alloced;
	}
	curr = heap_list.head;
	while (curr) {
		if (curr + 1 == ptr)
			break;
		curr = curr->next;
	}
	coalesce_blocks();
	if (curr) {
		if (curr->status == STATUS_FREE)
			return NULL;
		if (block_size >= MMAP_THRESHOLD) {
			alloced = os_malloc(size);
			curr->status = STATUS_FREE;
			memcpy(alloced, curr + 1, curr->size);
			return alloced;
		}
		if (block_size - HEADER_SIZE <= curr->size) {
			if (curr->size - block_size + HEADER_SIZE >= HEADER_SIZE + 8) {
				alloced = curr;
				block = alloced + block_size;
				block->next = curr->next;
				block->prev = curr;
				block->size = curr->size - block_size;
				block->status = STATUS_FREE;
				curr->next = block;
				if (block->next)
					block->next->prev = block;
				curr->size = block_size - HEADER_SIZE;
				heap_list.size++;
			}
			return curr + 1;
		}
		if (curr->next) {
			block = curr->next;
			if (block->status == STATUS_FREE) {
				curr->next = block->next;
				if (block->next)
					block->next->prev = curr;
				curr->size += block->size + HEADER_SIZE;
				if (curr->size - block_size + HEADER_SIZE >= HEADER_SIZE + 8 && curr->size >= block_size) {
					alloced = curr;
					block = alloced + block_size;
					block->next = curr->next;
					block->prev = curr;
					block->size = curr->size - block_size;
					block->status = STATUS_FREE;
					curr->next = block;
					if (block->next)
						block->next->prev = block;
					curr->size = block_size - HEADER_SIZE;
					heap_list.size++;
				}
				if (curr->size >= block_size - HEADER_SIZE)
					return curr + 1;
				curr->status =  STATUS_FREE;
				alloced = os_malloc(size);
				memcpy(alloced, curr + 1, curr->size);
				return alloced;
			}
			alloced = os_malloc(size);
			memcpy(alloced, curr + 1, curr->size);
			curr->status = STATUS_FREE;
			return alloced;
		}
		sbrk(block_size - curr->size - HEADER_SIZE);
		curr->size = block_size - HEADER_SIZE;
		return curr + 1;
	}
	return NULL;
}
