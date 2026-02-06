#include <core/log.h>
#include <core/platform.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static size_t _page_size;
static uintptr_t _page_mask;

__attribute__((constructor)) static void init_page_size(void) {
	_page_size = sysconf(_SC_PAGESIZE);
	_page_mask = _page_size - 1;
}

static size_t next_power_of_two(size_t n) {
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;

	return n++;
}

size_t align_up(size_t value, size_t alignment) {
	assert(alignment != 0);

	// Alignment must be a power of two. Clamp it otherwise
	if ((alignment & (alignment - 1)) != 0) {
		LOG_WARN("trying to align %zu to non-power of 2 %zu (clamping)", value,
				 alignment);
		alignment = next_power_of_two(alignment);
	}

	size_t res;
	return __builtin_uaddl_overflow(alignment - 1, value, &res) == 0
			   ? res & ~(alignment - 1)
			   : 0;
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
		LOG_ERR("platform_memory_reserve: mmap call failed");
		return false;
	}
	return addr;
}

bool platform_memory_dereserve(void* memory, size_t size) {
	int res = munmap(memory, size);
	if (res != 0) {
		LOG_ERR("platform_memory_dereserve: munmap call failed");
		return false;
	}
	return true;
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
		mprotect(memory, align_up(size, _page_size), PROT_READ | PROT_WRITE);
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
		mmap(memory, align_up(size, _page_size), PROT_NONE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, 0);

	if (ret == MAP_FAILED) {
		LOG_ERR("platform_memory_decommit: mmap call failed");
		return false;
	}
	return true;
}
