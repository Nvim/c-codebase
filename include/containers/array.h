#ifndef ARRAY_H
#define ARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <visibility.h>

typedef struct arena_t arena_t;

typedef struct array_t {
	void* internal;
} array_t;

LIB_EXPORT array_t* array_create(arena_t* arena, size_t size, size_t stride);

LIB_EXPORT void* array_at(array_t* array, size_t index);

LIB_EXPORT bool array_set(array_t* array, size_t index, void* value);

LIB_EXPORT size_t array_remaining(array_t* array);

LIB_EXPORT size_t array_usage(array_t* array);

LIB_EXPORT void array_clear(array_t* array);

#define array_create_typed(arena, size, type) array_create(arena, size, sizeof(type)

#endif // !ARRAY_H
