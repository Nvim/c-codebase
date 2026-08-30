#include <platform/platform.h>

#include <common.h>
#include <core/log.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static size_t _page_size;
static uintptr_t _page_mask;

__attribute__((constructor)) static void init_page_size(void) {
	_page_size = sysconf(_SC_PAGESIZE);
	_page_mask = _page_size - 1;
}

size_t platform_memory_page_size(void) { return _page_size; }

void* platform_memory_page_begin(void* addr) {
	return (void*)((uintptr_t)addr & ~_page_mask);
}

void* platform_memory_next_page(void* addr) {
	return (void*)((((uintptr_t)addr | _page_mask) + 1));
}

void* platform_memory_page_end(void* addr) {
	return (void*)(((uintptr_t)addr | _page_mask));
}

void* platform_memory_alloc(size_t size) { return malloc(size); }

void platform_memory_free(void* memory) { free(memory); }

void* platform_memory_reserve(size_t size) {
	void* addr = mmap(NULL, align_up(size, _page_size), PROT_NONE,
					  MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

	if (addr == MAP_FAILED) {
		LOG_ERR("mmap call failed");
		return false;
	}
	return addr;
}

bool platform_memory_dereserve(void* memory, size_t size) {
	int res = munmap(memory, size);
	if (res != 0) {
		LOG_ERR("munmap call failed");
		return false;
	}
	return true;
}

bool platform_memory_commit(void* memory, size_t size) {
	if (!memory || !size) {
		LOG_ERR("invalid parameters");
		return false;
	}
	if (platform_memory_page_begin(memory) != memory) {
		LOG_ERR("invalid beginning address");
		return false;
	}

	int ret =
		mprotect(memory, align_up(size, _page_size), PROT_READ | PROT_WRITE);
	if (ret != 0) {
		LOG_ERR("%d", ret);
		return false;
	}
	return true;
}

bool platform_memory_decommit(void* memory, size_t size) {
	if (!memory || !size) {
		LOG_ERR("invalid parameters");
		return false;
	}
	if (platform_memory_page_begin(memory) != memory) {
		LOG_ERR("invalid beginning address");
		return false;
	}

	void* ret =
		mmap(memory, align_up(size, _page_size), PROT_NONE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, 0);

	if (ret == MAP_FAILED) {
		LOG_ERR("mmap call failed");
		return false;
	}
	return true;
}
