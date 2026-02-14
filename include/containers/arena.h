#ifndef ARENA_H
#define ARENA_H

#include "visibility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct arena_t arena_t;
typedef struct arena_checkpoint arena_checkpoint;

typedef enum arena_flags {
	ARENA_GROWING = 1 << 0,			// Arena will automatically grow when needed
	ARENA_ZERO_ALWAYS = 1 << 1,		// Memory will always be zeroed out
	ARENA_LOG = 1 << 2,				// Debug logs will be printed
	ARENA_TRACE = 1 << 3,			// Arena state will be dumped to file
	ARENA_EXTERNAL_MEMORY = 1 << 4, // Memory has been allocated externally
	ARENA_FLAGS_COUNT = 1 << 5
} arena_flags;
#define ARENA_DUMP_PATH "arena.log"

typedef struct arena_create_info {
	size_t initial_size; // Allocation size
	size_t max_size;	 // Ignored if growable bit isn't set
	size_t alignment;
	arena_flags flags;
} arena_create_info;

/**
 * @brief Creates a new fixed size arena with a default alignment of
 * sizeof(uintptr_t).
 *
 * @param size The amount of underlying memory (may be rounded up for alignment)
 */
LIB_EXPORT arena_t* arena_create(size_t size);

/**
 * @brief Creates an new fixed_size arena, allocating `size` bytes of memory.
 *
 * @param size The amount of underlying memory
 * @param alignment The default memory alignment to use for this arena
 */
LIB_EXPORT arena_t* arena_create_aligned(size_t size, size_t alignment);

/**
 * @brief Creates an arena using the settings from `info`
 * @param info Arena settings. Can't be externally owned.
 */
LIB_EXPORT arena_t* arena_create_from_config(arena_create_info info);

/**
 * @brief Creates a new arena from an existing chunk of memory.
 *
 * @param Memory pointer to the underlying memory to be used
 * @param info Other arena settings. Can't be growable.
 *
 * [NOTE]: The arena's capacity will be less than the passed size, in order to
 * fit the arena header at the beginning of the block. Add `arena_header_size`
 * to your desired allocation size, making sure it matches desired alignment.
 * [NOTE]: Memory won't be initially touched, even if `ARENA_ZERO_ALWAYS` is
 * passed.
 */
LIB_EXPORT arena_t* arena_create_from_memory(void* memory,
											 arena_create_info info);

/**
 * @brief Size of the internal header at the beginning of every arena's memory
 * block. Use this to query the right size when pre-allocating a buffer.
 * [NOTE]: This size might not match your alignment requirements.
 */
LIB_EXPORT size_t arena_header_size(void);

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
 * Convinient macros for pushing data
 */
#define arena_push(arena, type) (type*)arena_alloc(arena, sizeof(type))
#define arena_push_array(arena, type, count) (type*)arena_alloc_array(arena, sizeof(type), count)

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
 * @brief Destroy the arena. The underlying memory is released
 */
LIB_EXPORT void arena_destroy(arena_t* arena);

/**
 * @brief Get current usage of arena in bytes
 *
 */
LIB_EXPORT size_t arena_get_usage(arena_t* arena);

/**
 * @brief Get beginning address of usable memory.
 *
 */
LIB_EXPORT void* arena_get_base(arena_t* arena);

/**
 * @brief Get remaining amount of memory in bytes.
 *
 * @note: If arena is growable, returns amount before next growth operation.
 */
LIB_EXPORT size_t arena_get_remaining(arena_t* arena);

LIB_EXPORT arena_checkpoint* arena_get_checkpoint(arena_t* arena);

LIB_EXPORT void arena_restore_checkpoint(arena_t* arena,
										 arena_checkpoint* checkpoint,
										 bool zero);

#endif // !ARENA_H
