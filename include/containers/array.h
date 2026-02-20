#ifndef ARRAY_H
#define ARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <visibility.h>

typedef struct arena_t arena_t;

typedef struct array_t {
	void* internal;
} array_t;

/**
 * @brief Create a new array
 *
 * @param arena Arena to allocate memory.
 * @param size Number of elements in the array.
 * @param elem_size Size of a single element.
 * @return The new array, or NULL on failure.
 */
LIB_EXPORT array_t* array_create(arena_t* arena, size_t size, size_t elem_size);

/**
 * @brief Returns the size of an array, or 0 if invalid array.
 */
LIB_EXPORT size_t array_size(array_t* array);

/**
 * @brief Returns the size of a single element of the array, or 0 if invalid
 * array.
 */
LIB_EXPORT size_t array_stride(array_t* array);

/**
 * @brief Access an element by index. Element is copied.
 *
 * @param out Pointer where result will be copied. Mustn't be null
 * @return False on invalid parameters, otherwise true.
 */
LIB_EXPORT bool array_get(array_t* array, void* out, size_t index);

/**
 * @brief Set the value of an element by index. Element is copied.
 *
 * @param value The value to copy in the array.
 * @return False on invalid parameters, otherwise true.
 */
LIB_EXPORT bool array_set(array_t* array, size_t index, void* value);

/**
 * @brief Clears the array. All memory will be zeroed-out.
 *
 * @param array The array to clear.
 */
LIB_EXPORT void array_clear(array_t* array);

#define array_create_typed(arena, size, type) array_create(arena, size, sizeof(type))

#endif // !ARRAY_H
