#include "arena_internal.h"
#include <arena.h>

#include "common/util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

arena_t* arena_create(size_t size) {
	return arena_create_aligned(size, ALIGNMENT);
}

arena_t* arena_create_aligned(size_t size, size_t alignment) {
	size = align_up(size, alignment);
	void* memory = malloc(size + sizeof(arena_impl));
	if (!memory) {
		LOG_ERR("arena allocation failed");
		return NULL;
	}
	arena_impl* impl = memory;
	{
		impl->begin = (char*)memory + sizeof(arena_impl);
		impl->current = impl->begin;
		impl->allocation_size = size;
		impl->alignment = alignment;
	}

	return impl->begin;
}

void* arena_alloc(arena_t* arena, size_t size) {
	if (!arena) {
		LOG_ERR("invalid arena handle");
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	size = align_up(size, impl->alignment);
	size_t remaining = arena_impl_remaining(impl);
	if (remaining < size) {
		// TODO: growable
		LOG_ERR("not enough space for %zu. (%zu left)");
		return NULL;
	}

	impl->current = (char*)impl->current + size;

	return impl->current;
}

void* arena_alloc_array(arena_t* arena, size_t elem_size, size_t elem_count) {
	if (!arena) {
		LOG_ERR("invalid arena handle");
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	size_t size = align_up(elem_size * elem_count, impl->alignment);
	size_t remaining = arena_impl_remaining(impl);
	if (remaining < size) {
		// TODO: growable
		LOG_ERR("not enough space for %zu. (%zu left)");
		return NULL;
	}

	impl->current = (char*)impl->current + size;

	return impl->current;
}

void* arena_reset(arena_t* arena) {
	if (!arena) {
		LOG_ERR("invalid arena handle");
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	impl->current = impl->begin;
	return impl->current;
}

void* arena_reset_zero(arena_t* arena) {
	if (!arena) {
		LOG_ERR("invalid arena handle");
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	impl->current = impl->begin;
	memset(impl->begin, 0, impl->allocation_size);

	return impl->current;
}

void arena_destroy(arena_t* arena) {
	if (!arena) {
		LOG_ERR("invalid arena handle");
		return;
	}
	arena_impl* impl = (arena_impl*)arena;
	if (!impl->begin || !impl->allocation_size) {
		LOG_INFO("arena has no allocation");
		return;
	}
	free(impl->begin);
}
