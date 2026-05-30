#ifndef POOL_H
#define POOL_H

#include "core/core.h"

#include <allocators/arena.h>
#include <containers/array.h>

#include <stdint.h>

/**
 * @typedef pool_t
 * @brief Generic Data-Oriented object pool container.
 * Data is stored in a contiguous array, and allocation,
 * access and removal are all O(1) thanks to a free
 * list and generation system.
 * The pool is used with opaque handles that encode the index and generation so
 * checking validity is also trivilal.
 *
 */
typedef struct pool_t pool_t;

typedef struct {
	uint32_t handle;
} pool_handle;

#define POOL_INVALID_IDX 0
#define POOL_INVALID_HANDLE                                                    \
	(pool_handle) { .handle = POOL_INVALID_IDX }

static inline bool check_handle(pool_handle handle) {
	return handle.handle != POOL_INVALID_IDX;
}

ARRAY_DEFINE_TYPE(pool_handle, pool_handle_array)

/**
 * @brief Create a new pool of a given size.
 *
 * @param arena Allocate the backing memory. Pool lifetime is tied to this
 * @param pool_size Number of elements desired.
 * @param elem_size Size of a single element.
 *
 */
LIB_EXPORT pool_t* pool_create(arena_t* arena, uint32_t pool_size,
							   uint32_t elem_size);

/**
 * @brief Allocate a new element from the pool.
 *
 * @param elem The new element's data, which will be copied in the pool's array.
 *
 */
LIB_EXPORT pool_handle pool_new(pool_t* pool, void* elem);

/**
 * @brief Release a pool element. Simply marks it as unused.
 *
 * @param handle The element to release. Will be invalidated after this call.
 *
 */
LIB_EXPORT void pool_release(pool_t* pool, pool_handle handle);

/**
 * @brief Verify if a pool handle is still valid.
 *
 */
LIB_EXPORT bool pool_is_handle_valid(pool_t* pool, pool_handle handle);

LIB_EXPORT uint32_t pool_handle_index(pool_handle handle);

/**
 * @brief Get a pointer to a pool element.
 *
 */
LIB_EXPORT void* pool_get(pool_t* pool, pool_handle handle);

/**
 * @brief Invalidates all existing handles.
 *
 */
LIB_EXPORT void pool_reset(pool_t* pool);

/* -- Type-safe wrapper: -- */
#define POOL_DEFINE_TYPE(T, Name)                                              \
	typedef struct Name {                                                      \
		pool_t* raw;                                                           \
	} Name;                                                                    \
                                                                               \
	typedef struct {                                                           \
		pool_handle raw_handle;                                                \
	} Name##_handle;                                                           \
                                                                               \
	static inline pool_handle Name##_unwrap(Name##_handle r) {                 \
		return r.raw_handle;                                                   \
	}                                                                          \
                                                                               \
	static inline Name##_handle Name##_wrap(pool_handle h) {                   \
		return (Name##_handle){.raw_handle = h};                               \
	}                                                                          \
                                                                               \
	UNUSED static inline Name Name##_create(arena_t* arena,                    \
											uint32_t pool_size) {              \
		Name ret = {pool_create(arena, pool_size, sizeof(T))};                 \
		return ret;                                                            \
	}                                                                          \
	UNUSED static inline Name##_handle Name##_new(Name pool, T* elem) {        \
		return Name##_wrap(pool_new(pool.raw, elem));                          \
	}                                                                          \
	UNUSED static inline void Name##_release(Name pool,                        \
											 Name##_handle handle) {           \
		pool_release(pool.raw, Name##_unwrap(handle));                         \
	}                                                                          \
                                                                               \
	UNUSED static inline bool Name##_is_handle_valid(Name pool,                \
													 Name##_handle handle) {   \
		return pool_is_handle_valid(pool.raw, Name##_unwrap(handle));          \
	}                                                                          \
                                                                               \
	UNUSED static inline T* Name##_get(Name pool, Name##_handle handle) {      \
		return (T*)pool_get(pool.raw, Name##_unwrap(handle));                  \
	}                                                                          \
                                                                               \
	UNUSED static inline void Name##_reset(Name pool) { pool_reset(pool.raw); }

#endif // !POOL_H
