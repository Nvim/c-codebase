#include "arena_internal.h"
#include <containers/arena.h>
#include <core/platform.h>
#include <core/log.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG(impl, msg, ...)                                                  \
	{                                                                          \
		if (impl->flags & ARENA_LOG) { LOG_DEBUG(msg, ##__VA_ARGS__); }        \
	}

static arena_impl* _arena_create_internal(arena_create_info info) {
	bool growable = info.flags & ARENA_GROWING;

	if (growable) {
		if (info.max_size == 0 || info.max_size < info.alignment) {
			LOG_ERR("arena_create: alignment");
			return NULL;
		}
	} else {
		if (info.initial_size == 0) {
			LOG_ERR("arena_create: invalid initial size");
			return NULL;
		}
	}

	size_t size_align = align_up(info.initial_size, info.alignment);

	// [TODO]: mmap reserve if growable

	/* [NOTE]: Because of alignment, 1st allocation may be not be right after
	 * header, so we align requested size AND header size.
	 * For example, here, header is 48 bytes, but a 32 byte alignment
	 * will require begin on 64:
	 * [HEADER] [pad] [BEGIN]
	 * 0    47    63  64
	 */
	void* memory =
		malloc(size_align + align_up(ARENA_HEADER_SIZE, info.alignment));
	if (!memory) {
		LOG_ERR("arena_create: arena allocation failed");
		return NULL;
	}

	size_t begin = (size_t)memory + ARENA_HEADER_SIZE;
	begin = align_up(begin, info.alignment);
	arena_impl* impl = memory;
	{
		impl->flags = info.flags;
		impl->alignment = info.alignment;
		impl->max_size = info.max_size;
		impl->allocation_size = size_align;
		impl->begin = (void*)begin;
		impl->current = impl->begin;
	}

	if (impl->flags & ARENA_ZERO_ALWAYS) {
		memset(impl->begin, 0, impl->allocation_size);
	}

	DEBUG(impl, "arena_create: allocated: %zu (%zu requested)", size_align,
		  info.initial_size);
	return impl;
}

arena_t* arena_create(size_t size) {
	arena_create_info info;
	memset(&info, 0, sizeof(info));
	info.initial_size = size;
	info.alignment = ALIGNMENT;
	return (arena_t*)_arena_create_internal(info);
}

arena_t* arena_create_aligned(size_t size, size_t alignment) {
	arena_create_info info;
	memset(&info, 0, sizeof(info));
	info.initial_size = size;
	info.alignment = alignment;
	return (arena_t*)_arena_create_internal(info);
}

arena_t* arena_create_from_config(arena_create_info info) {
	if (info.flags & ARENA_EXTERNAL_MEMORY) {
		LOG_ERR("arena_create_from_config: invalid flags");
		return NULL;
	}
	return (arena_t*)_arena_create_internal(info);
}

arena_t* arena_create_from_memory(void* memory, arena_create_info info) {
	if (!memory) {
		LOG_ERR("arena_create_from: invalid memory");
		return NULL;
	}

	if (info.flags & ARENA_GROWING) {
		LOG_ERR("arena_create_from_memory: invalid flags");
		return NULL;
	}

	size_t size = info.initial_size;
	uint8_t alignment = info.alignment;

	if (size <= ARENA_HEADER_SIZE || size % alignment != 0) {
		LOG_ERR("arena_create_from: memory is too short or not aligned");
		return NULL;
	}

	size_t begin = (size_t)memory + ARENA_HEADER_SIZE;
	begin = align_up(begin, info.alignment);
	arena_impl* impl = memory;
	{
		impl->begin = (void*)begin;
		impl->current = impl->begin;
		impl->allocation_size = size - ARENA_HEADER_SIZE;
		impl->alignment = alignment;
		impl->flags = info.flags | ARENA_EXTERNAL_MEMORY;
	}

	return (arena_t*)impl;
}

size_t arena_header_size(void) { return ARENA_HEADER_SIZE; }

void* arena_alloc(arena_t* arena, size_t size) {
	if (!arena) {
		LOG_ERR("arena_alloc: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	size = align_up(size, impl->alignment);
	DEBUG(impl, "arena_alloc: allocating %zu", size);

	size_t remaining = arena_impl_remaining(impl);
	if (remaining < size) {
		// [TODO]: growable
		LOG_ERR("arena_alloc: not enough space for %zu. (%zu left)", size,
				arena_impl_remaining(impl));
		return NULL;
	}

	void* ret = impl->current;
	impl->current = (char*)impl->current + size;
	return ret;
}

void* arena_alloc_array(arena_t* arena, size_t elem_size, size_t elem_count) {
	if (!arena) {
		LOG_ERR("arena_alloc_array: invalid arena handle: %p", arena);
		return NULL;
	}
	size_t size = elem_size * elem_count;
	return arena_alloc(arena, size);
}

void* arena_reset(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_reset: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	impl->current = impl->begin;
	if (impl->flags & ARENA_ZERO_ALWAYS) {
		memset(impl->begin, 0, impl->allocation_size);
	}
	return impl->current;
}

void* arena_reset_zero(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_reset_zero: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	impl->current = impl->begin;
	memset(impl->begin, 0, impl->allocation_size);

	return impl->current;
}

void arena_destroy(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_destroy: invalid arena handle: %p", arena);
		return;
	}
	arena_impl* impl = (arena_impl*)arena;

	if (impl->flags & ARENA_EXTERNAL_MEMORY) { return; }

	if (!impl->begin || !impl->allocation_size) {
		LOG_INFO("arena has no allocation");
		return;
	}

	if (impl->flags & ARENA_ZERO_ALWAYS) {
		memset(impl->begin, 0, impl->allocation_size);
	}
	free(arena);
}

size_t arena_get_usage(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_get_usage: invalid arena handle: %p", arena);
		return 0;
	}
	arena_impl* impl = (arena_impl*)arena;

	size_t used = (char*)impl->current - (char*)impl->begin;
	return used;
}

size_t arena_get_remaining(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_get_remaining: invalid arena handle: %p", arena);
		return 0;
	}
	arena_impl* impl = (arena_impl*)arena;

	size_t remaining = arena_impl_remaining(impl);
	return remaining;
}

arena_checkpoint* arena_get_checkpoint(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_get_checkpoint: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	DEBUG(impl, "setting checkpoint at %p.", impl->current);
	return impl->current;
}

void arena_restore_checkpoint(arena_t* arena, arena_checkpoint* checkpoint,
							  bool zero) {
	if (!arena) {
		LOG_ERR("arena_restore_checkpoint: invalid arena handle: %p", arena);
		return;
	}
	arena_impl* impl = (arena_impl*)arena;
	void* p = checkpoint;

	if (impl->current == p) { return; }

	if ((char*)impl->current < (char*)p) {
		LOG_ERR("arena_restore_checkpoint: invalid checkpoint: %p", checkpoint);
		return;
	}

	void* prev = impl->current;
	size_t offset = (char*)p - (char*)prev;
	impl->current = p;
	if (zero || impl->flags & ARENA_ZERO_ALWAYS) {
		memset(impl->current, 0, offset);
	}
}
