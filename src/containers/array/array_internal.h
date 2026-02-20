#ifndef ARRAY_INTERNAL_H
#define ARRAY_INTERNAL_H

#include <core/platform.h>

#include <stddef.h>

typedef struct array_impl {
	size_t size;
	size_t elem_size;
	void* memory;
	size_t _pad[1];
} array_impl;

STATIC_ASSERT(sizeof(array_impl) == 32, "bad array size");

#endif // !ARRAY_INTERNAL_H
