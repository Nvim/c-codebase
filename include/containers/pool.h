#ifndef POOL_H
#define POOL_H

#include "core/core.h"
#include <allocators/arena.h>

#include <stdint.h>

typedef struct pool_t pool_t;

typedef struct {
	uint32_t handle;
} pool_handle;

#define POOL_INVALID_HANDLE 0

LIB_EXPORT pool_t* pool_create(arena_t* arena, uint32_t pool_size,
							   uint32_t elem_size);

// memcpy!
LIB_EXPORT pool_handle pool_new(pool_t* pool, void* elem);

LIB_EXPORT void pool_release(pool_t* pool, pool_handle handle);

LIB_EXPORT bool pool_is_handle_valid(pool_t* pool, pool_handle handle);

LIB_EXPORT void* pool_get(pool_t* pool, pool_handle handle);

LIB_EXPORT void pool_reset(pool_t* pool);

//-- Type-safe wrapper: --//
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
