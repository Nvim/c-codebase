#include <containers/pool.h>

#include <core/core.h>
#include <core/log.h>
#include <string.h>

// 32-bit handle layout:
// [ generation (12 bits) | index (20 bits) ]
#define POOL_INDEX_BITS 20
#define POOL_GENERATION_BITS 12
#define POOL_INDEX_MASK ((1u << POOL_INDEX_BITS) - 1)
#define POOL_GENERATION_MASK ((1u << POOL_GENERATION_BITS) - 1)

typedef uint16_t gen_t;
typedef uint32_t idx_t;

static inline uint32_t _handle_create(idx_t index, gen_t generation) {
	return ((generation & POOL_GENERATION_MASK) << POOL_INDEX_BITS) |
		   (index & POOL_INDEX_MASK);
}

static inline idx_t _handle_index(uint32_t handle) {
	return handle & POOL_INDEX_MASK;
}

static inline gen_t _handle_generation(uint32_t handle) {
	return (handle >> POOL_INDEX_BITS) & POOL_GENERATION_MASK;
}

static inline uint32_t _pool_alloc_size(uint32_t elem_count,
										uint32_t elem_size) {
	uint32_t result;
	if (__builtin_mul_overflow(elem_count, elem_size, &result)) {
		ASSERT(false);
		return 0;
	}
	return result;
}

static inline uint32_t _pool_gen_alloc_size(uint32_t elem_count) {
	uint32_t result;
	if (__builtin_mul_overflow(elem_count, (uint32_t)sizeof(gen_t), &result)) {
		ASSERT(false);
		return 0;
	}
	return result;
}

static inline uint32_t _pool_freelist_alloc_size(uint32_t elem_count) {
	uint32_t result;
	if (__builtin_mul_overflow(elem_count, (uint32_t)sizeof(uint32_t),
							   &result)) {
		ASSERT(false);
		return 0;
	}
	return result;
}

/**
 * @typedef pool_impl
 * @brief Pool internal.
 * - Freed handles are recycled by inrecementing their slot
 * in `recyle_counts`. The free list behaves like a linked list and contains
 * indices to the next free element
 * - A valid X is an X whose generation is higher than its recycle count.
 * - Creating an X increment its generation.
 * - Destroying an X increments the recycle counter
 */
typedef struct pool_impl {
	void* elems;
	gen_t* recycle_counts;
	uint32_t* free_list;
	uint32_t first_free;
	uint32_t free_count;
	uint32_t pool_elem_count;
	uint32_t elem_size;
} pool_impl;

STATIC_ASSERT(sizeof(pool_impl) == 40, "");

LIB_EXPORT pool_t* pool_create(arena_t* arena, uint32_t pool_size,
							   uint32_t elem_size) {
	if (!arena || !pool_size || !elem_size) {
		LOG_ERR("pool_create: invalid parameters");
		return NULL;
	}

	// [NOTE]: we reserve an invalid index but still want user to have access to
	// all requested slots
	pool_size++;

	uint32_t alloc_size = _pool_alloc_size(pool_size, elem_size);
	uint32_t gen_alloc_size = _pool_gen_alloc_size(pool_size);
	uint32_t freelist_alloc_size = _pool_freelist_alloc_size(pool_size);

	if (arena_get_growth_remaining(arena) <
		alloc_size + gen_alloc_size + freelist_alloc_size + sizeof(pool_impl)) {
		LOG_ERR("pool_create: not enough space in arena");
		LOG_ERR("... want: %zu", alloc_size + gen_alloc_size +
									 freelist_alloc_size + sizeof(pool_impl));
		LOG_ERR("... have: %zu", arena_get_growth_remaining(arena));
		return NULL;
	}

	pool_impl* ret = arena_push_z(arena, pool_impl);
	if (!ret) {
		LOG_ERR("pool_create: couldn't allocate pool");
		return NULL;
	}

	void* elems = arena_alloc_clear(arena, alloc_size, 0);
	if (!elems) {
		LOG_ERR("pool_create: couldn't allocate pool array");
		return false;
	}
	gen_t* gens = arena_push_array_z(arena, gen_t, pool_size);
	if (!gens) {
		LOG_ERR("pool_create: couldn't allocate generations array");
		return false;
	}

	uint32_t* freelist = arena_push_array_z(arena, uint32_t, pool_size);
	if (!freelist) {
		LOG_ERR("pool_create: couldn't allocate freelist array");
		return false;
	}

	for (uint32_t i = 1; i < pool_size - 1; ++i) { freelist[i] = i + 1; }
	freelist[0] = POOL_INVALID_HANDLE;
	freelist[pool_size - 1] = POOL_INVALID_HANDLE;

	{
		ret->elems = elems;
		ret->recycle_counts = gens;
		ret->free_list = freelist;
		ret->first_free = 1;			 // 0 is invalid
		ret->free_count = pool_size - 1; // 0 is reserved
		ret->pool_elem_count = pool_size;
		ret->elem_size = elem_size;
	}

	return (pool_t*)ret;
}

pool_handle pool_new(pool_t* pool, void* elem) {
	if (!pool || !elem) {
		LOG_ERR("pool_new: invalid parameters");
		return (pool_handle){.handle = POOL_INVALID_HANDLE};
	}

	pool_impl* impl = (pool_impl*)pool;
	if (!impl->free_count || impl->first_free == POOL_INVALID_HANDLE) {
		LOG_ERR("pool_new: pool is full");
		return (pool_handle){.handle = POOL_INVALID_HANDLE};
	}

	uint32_t idx = impl->first_free;
	gen_t generation = impl->recycle_counts[idx] + 1;
	pool_handle ret = {.handle = _handle_create(idx, generation)};
	ASSERT(pool_is_handle_valid(pool, ret));

	void* addr = (uint8_t*)impl->elems + ((size_t)idx * impl->elem_size);
	memcpy(addr, elem, impl->elem_size);

	impl->first_free = impl->free_list[idx];
	impl->free_count--;

	return ret;
}

bool pool_is_handle_valid(pool_t* pool, pool_handle handle) {
	if (!pool) {
		LOG_ERR("pool_is_handle_valid: invalid parameters");
		return false;
	}
	pool_impl* impl = (pool_impl*)pool;

	idx_t idx = _handle_index(handle.handle);
	gen_t gen = _handle_generation(handle.handle);

	if (gen == 0) { return false; }
	if (gen > impl->recycle_counts[idx] + 1) { return false; }
	if (idx == POOL_INVALID_HANDLE) { return false; }
	if (idx >= impl->pool_elem_count) { return false; }
	if (gen <= impl->recycle_counts[idx]) { return false; }

	return true;
}

void* pool_get(pool_t* pool, pool_handle handle) {
	if (!pool_is_handle_valid(pool, handle)) { return NULL; }

	pool_impl* impl = (pool_impl*)pool;
	uint32_t idx = _handle_index(handle.handle);
	void* addr = (uint8_t*)impl->elems + (impl->elem_size * idx);
	return addr;
}

void pool_release(pool_t* pool, pool_handle handle) {
	if (!pool) {
		LOG_ERR("pool_release: invalid parameters");
		return;
	}

	if (!pool_is_handle_valid(pool, handle)) {
		LOG_ERR("pool_release: invalid handle");
		return;
	}

	pool_impl* impl = (pool_impl*)pool;
	idx_t idx = _handle_index(handle.handle);

	impl->recycle_counts[idx]++;

	impl->free_list[idx] = impl->first_free;
	impl->first_free = idx;

	impl->free_count++;
}

void pool_reset(pool_t* pool) {
	if (!pool) {
		LOG_ERR("pool_reset: invalid parameters");
		return;
	}

	pool_impl* impl = (pool_impl*)pool;

	// Clear elements
	memset(impl->elems, 0,
		   _pool_alloc_size(impl->pool_elem_count, impl->elem_size));

	// Invalidate all handles by bumping generations
	for (uint32_t i = 1; i < impl->pool_elem_count; ++i) {
		impl->recycle_counts[i]++;
		if (impl->recycle_counts[i] == 0) {
			impl->recycle_counts[i] = 1; // avoid 0
		}
	}

	// Rebuild freelist
	for (uint32_t i = 1; i < impl->pool_elem_count - 1; ++i) {
		impl->free_list[i] = i + 1;
	}
	impl->free_list[impl->pool_elem_count - 1] = POOL_INVALID_HANDLE;

	impl->first_free = 1;
	impl->free_count = impl->pool_elem_count;
}
