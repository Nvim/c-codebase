#ifndef ARENA_H
#define ARENA_H

#include "visibility.h"
#include <stddef.h>

typedef struct arena_t arena_t;

/**
 * @brief Creates an new arena, allocating `size` bytes of memory.
 *
 * @param size The amount of underlying memory
 */
LIB_EXPORT arena_t* arena_create(size_t size);

/**
 * @brief Creates an new arena, allocating `size` bytes of memory.
 *
 * @param size The amount of underlying memory
 * @param alignment The default memory alignment to use for this arena
 */
LIB_EXPORT arena_t* arena_create_aligned(size_t size, size_t alignment);

/**
 * @brief Allocate memory from the arena.
 *
 * @param size The amount of memory to allocate
 */
LIB_EXPORT void* arena_alloc(arena_t* arena, size_t size);

/**
 * @brief Allocate enough memory for an array of specified type
 *
 * @param elem_size Size of one array element
 * @param elem_count Number of array element to allocate
 */
LIB_EXPORT void* arena_alloc_array(arena_t* arena, size_t elem_size,
						size_t elem_count);

/**
 * @brief Restore the arena to it's original state. No memory is freed
 *
 * @return The beginning of arena
 */
LIB_EXPORT void* arena_reset(arena_t* arena);

/**
 * @brief Restore the arena to it's original state. All memory is filled with 0.
 *
 */
LIB_EXPORT void* arena_reset_zero(arena_t* arena);

/**
 * @brief Desctoy the arena. The underlying memory is released
 *
 */
LIB_EXPORT void arena_destroy(arena_t* arena);

/**
 * @brief Get current usage of arena in bytes
 *
 */
LIB_EXPORT size_t arena_get_usage(arena_t* arena);

/**
 * @brief Get remaining amount of memory before reallocation in bytes.
 *
 */
LIB_EXPORT size_t arena_get_remaining(arena_t* arena);

#endif // !ARENA_H
