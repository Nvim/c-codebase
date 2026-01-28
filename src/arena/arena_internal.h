#ifndef ARENA_INTERNAL_H
#define ARENA_INTERNAL_H

#include <arena.h>

#include <stddef.h>

#define ALIGNMENT sizeof(uintptr_t)

typedef struct arena_impl {
	void* begin;
	void* current;
	size_t allocation_size;
	size_t alignment;
} arena_impl;

static inline size_t arena_impl_total_size(arena_impl* arena) {
	return arena->allocation_size + sizeof(arena_impl);
}

static inline size_t arena_impl_remaining(arena_impl* arena) {
	return arena->allocation_size - (size_t)arena->current;
}

#endif // !ARENA_INTERNAL_H
