#ifndef ARENA_INTERNAL_H
#define ARENA_INTERNAL_H

#include <containers/arena.h>
#include <core/platform.h>

#include <stddef.h>

#define ALIGNMENT sizeof(uintptr_t)

typedef struct arena_impl {
	void* begin;
	void* current;
	size_t allocation_size; // Actual commited memory
	size_t max_size;		// Adress space upper bound
	uint8_t alignment;
	uint8_t flags;
	size_t _pad[3]; // Force power of 2 at compile-time
} arena_impl;

static size_t const ARENA_HEADER_SIZE = sizeof(arena_impl);
STATIC_ASSERT(sizeof(arena_impl) == 64ul, "Bad arena_impl size");

static inline size_t arena_impl_total_size(arena_impl* arena) {
	return arena->allocation_size + sizeof(arena_impl);
}

static inline size_t arena_impl_remaining(arena_impl* arena) {
	char* max = (char*)arena->begin + arena->allocation_size;
	char* current = arena->current;
	if (max <= current) { return 0; }
	return max - current;
}

#endif // !ARENA_INTERNAL_H
