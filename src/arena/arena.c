#include "arena_internal.h"
#include <arena.h>

#include "common/util.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

arena_t* arena_create(size_t size) {
	return arena_create_aligned(size, ALIGNMENT);
}

arena_t* arena_create_aligned(size_t size, size_t alignment) {
	size_t size_align = align_up(size, alignment);
	LOG_DEBUG("Size: %zu -- Aligned: %zu", size, size_align);
	assert(size_align >= size);

	void* memory = malloc(size_align + sizeof(arena_impl));
	if (!memory) {
		LOG_ERR("arena allocation failed");
		return NULL;
	}
	arena_impl* impl = memory;
	{
		impl->begin = (char*)memory + sizeof(arena_impl);
		impl->current = impl->begin;
		impl->allocation_size = size_align;
		impl->alignment = alignment;
	}

	return (arena_t*)impl;
}

void* arena_alloc(arena_t* arena, size_t size) {
	if (!arena) {
		LOG_ERR("arena_alloc: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	size = align_up(size, impl->alignment);
	LOG_DEBUG("arena_alloc: allocating %zu", size);
	size_t remaining = arena_impl_remaining(impl);
	if (remaining < size) {
		// TODO: growable
		LOG_ERR("arena_alloc: not enough space for %zu. (%zu left)", size,
				arena_impl_remaining(impl));
		return NULL;
	}

	impl->current = (char*)impl->current + size;

	return impl->current;
}

void* arena_alloc_array(arena_t* arena, size_t elem_size, size_t elem_count) {
	if (!arena) {
		LOG_ERR("arena_alloc_array: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	size_t size = align_up(elem_size * elem_count, impl->alignment);
	size_t remaining = arena_impl_remaining(impl);
	if (remaining < size) {
		// TODO: growable
		LOG_ERR("arena_alloc_array: not enough space for %zu. (%zu left)", size,
				arena_impl_remaining(impl));
		return NULL;
	}

	impl->current = (char*)impl->current + size;

	return impl->current;
}

void* arena_reset(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_reset: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	impl->current = impl->begin;
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

	if (!impl->begin || !impl->allocation_size) {
		LOG_INFO("arena has no allocation");
		return;
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
