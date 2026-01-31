#include <platform.h>

#include "common/util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static size_t g_page_size;
static uintptr_t g_page_mask;

__attribute__((constructor)) static void init_page_size(void) {
	g_page_size = sysconf(_SC_PAGESIZE);
	g_page_mask = g_page_size - 1;
}

size_t align_up(size_t value, size_t alignment) {
	assert((alignment & (alignment - 1)) == 0);
	size_t res;
	return __builtin_uaddl_overflow(alignment - 1, value, &res) == 0
			   ? res & ~(alignment - 1)
			   : 0;
}

size_t platform_memory_page_size(void) { return g_page_size; }

void* platform_memory_page_begin(void* addr) {
	return (void*)((uintptr_t)addr & ~g_page_mask);
}

void* platform_memory_next_page(void* addr) {
    return (void *)((((uintptr_t)addr | g_page_mask) + 1));
}

void* platform_memory_alloc(size_t size) { return malloc(size); }

void platform_memory_free(void* memory) { free(memory); }

void* platform_memory_reserve(size_t size) {
	void* addr = mmap(NULL, align_up(size, g_page_size), PROT_NONE,
					  MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

	if (addr == MAP_FAILED) {
		LOG_ERR("platform_memory_reserve: mmap call failed");
		return false;
	}
	return addr;
}

bool platform_memory_commit(void* memory, size_t size) {
	if (!memory || !size) {
		LOG_ERR("platform_memory_commit: invalid parameters");
		return false;
	}
	if (platform_memory_page_begin(memory) != memory) {
		LOG_ERR("platform_memory_commit: invalid beginning address");
		return false;
	}

	int ret =
		mprotect(memory, align_up(size, g_page_size), PROT_READ | PROT_WRITE);
	if (ret != 0) {
		LOG_ERR("platform_memory_commit: mprotect call failed: %d", ret);
		return false;
	}
	return true;
}

bool platform_memory_decommit(void* memory, size_t size) {
	if (!memory || !size) {
		LOG_ERR("platform_memory_decommit: invalid parameters");
		return false;
	}
	if (platform_memory_page_begin(memory) != memory) {
		LOG_ERR("platform_memory_decommit: invalid beginning address");
		return false;
	}

	void* ret =
		mmap(memory, align_up(size, g_page_size), PROT_NONE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, 0);

	if (ret == MAP_FAILED) {
		LOG_ERR("platform_memory_decommit: mmap call failed");
		return false;
	}
	return true;
}
