#ifndef HASHTABLE_INTERNAL_H
#define HASHTABLE_INTERNAL_H

#include <common.h>
#include <containers/hashtable.h>
#include <platform/platform.h>

typedef struct ht_impl {
	uint32_t max_allocs;  // Size requested by user
	uint32_t elem_size;	  // sizeof(elem)
	uint32_t alloc_count; // Allocation count
	void* memory; // Bigger than needed, so table is never fuller than HT_RATIO
	hash_fn func;
} ht_impl;

STATIC_ASSERT(sizeof(ht_impl) == 32, "");

static inline size_t ht_allocation_size(uint32_t count, uint32_t elem_size) {
	size_t max_size = (1.f / HT_RATIO) * (float)count;
	return align_up(max_size * elem_size, 8);
}

#endif // !HASHTABLE_INTERNAL_H
