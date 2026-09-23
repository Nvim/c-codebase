/**
 * @file array.h
 *
 * @brief array_t is a safe array type with bounds checking, similar to
 * std::array in C++.
 * The ARRAY_DEFINE_TYPE macro at the end of the file creates a type-safe
 * wrapper around the generic void* based interface.
 * Array types defined with the macro are recommended over the generic
 * functions, which is why they are all prefixed with an underscore.
 */

#ifndef ARRAY_H
#define ARRAY_H

#include <core/core.h>
#include <visibility.h>

#include <stdbool.h>
#include <stddef.h>

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
LIB_EXPORT array_t* _array_create(arena_t* arena, size_t size,
								  size_t elem_size);

/**
 * @brief Returns the size of an array, or 0 if invalid array.
 */
LIB_EXPORT size_t _array_size(array_t* array);

/**
 * @brief Returns the size of a single element of the array, or 0 if invalid
 * array.
 */
LIB_EXPORT size_t _array_stride(array_t* array);

/**
 * @brief Get a read/write pointer to element. No copy is performed
 *
 * @param out Pointer where address will be placed.
 * @return False on invalid parameters, otherwise true.
 */
LIB_EXPORT bool _array_at(array_t* array, void** out, size_t index);

/**
 * @brief Get a read-only pointer to element. No copy is performed
 *
 * @param out Pointer where address will be placed.
 * @return False on invalid parameters, otherwise true.
 */
LIB_EXPORT bool _array_at_const(array_t* array, void const** out, size_t index);

/**
 * @brief Access an element by index. Element is copied.
 *
 * @param out Pointer where result will be copied. Mustn't be null
 * @return False on invalid parameters, otherwise true.
 */
LIB_EXPORT bool _array_get(array_t* array, void* out, size_t index);

/**
 * @brief Set the value of an element by index. Element is copied.
 *
 * @param value The value to copy in the array.
 * @return False on invalid parameters, otherwise true.
 */
LIB_EXPORT bool _array_set(array_t* array, void* value, size_t index);

/**
 * @brief Clears the array. All memory will be zeroed-out.
 *
 * @param array The array to clear.
 */
LIB_EXPORT void _array_clear(array_t* array);

//-- Type-safe wrapper: --//
#define ARRAY_DEFINE_TYPE(T, Name)                                             \
                                                                               \
	typedef struct Name {                                                      \
		array_t* raw;                                                          \
	} Name;                                                                    \
                                                                               \
	__attribute__((unused)) static inline Name Name##_create(arena_t* arena,   \
															 size_t size) {    \
		Name arr = {_array_create(arena, size, sizeof(T))};                    \
		return arr;                                                            \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline size_t Name##_size(Name arr) {       \
		return _array_size(arr.raw);                                           \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_at(Name arr, T** out,    \
														 size_t index) {       \
		return _array_at(arr.raw, (void**)out, index);                         \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline T* Name##_begin(Name arr) {          \
		T* ret;                                                                \
		ASSERT(_array_at(arr.raw, (void**)&ret, 0));                           \
		return ret;                                                            \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_at_const(                \
		Name arr, T const** out, size_t index) {                               \
		return _array_at_const(arr.raw, (void const**)out, index);             \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_get(Name arr, T* out,    \
														  size_t index) {      \
		return _array_get(arr.raw, out, index);                                \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_set(                     \
		Name arr, size_t index, T* value) {                                    \
		return _array_set(arr.raw, (void*)value, index);                       \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline void Name##_clear(Name arr) {        \
		_array_clear(arr.raw);                                                 \
	}

#endif // !ARRAY_H
