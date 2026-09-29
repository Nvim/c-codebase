#include "hashtable_internal.h"

#include <allocators/arena.h>
#include <core/log.h>
#include <strings/string.h>

#include <limits.h>
#include <string.h>

static bool _is_elem_null(void const* elem, uint32_t elem_size) {
	static unsigned char const zeros[8] = {0};
	return memcmp(elem, zeros, elem_size) == 0;
}

hashtable_t* hashtable_create(arena_t* arena, uint32_t count,
							  uint32_t elem_size, hash_fn func) {
	if (!arena || count == 0 || elem_size == 0) {
		LOG_ERR("invalid parameters");
		return NULL;
	}

	// [TODO]: overflow check
	size_t allocation_size = ht_allocation_size(count, elem_size);

	if (arena_get_growth_remaining(arena) < sizeof(ht_impl) + allocation_size) {
		LOG_ERR("hashtable_create: not enough space left on arena");
		return NULL;
	}

	ht_impl* ret = arena_push(arena, ht_impl);
	if (!ret) {
		LOG_ERR("allocation failed");
		return NULL;
	}

	ret->memory = arena_alloc_clear(arena, allocation_size, 0);
	if (!ret->memory) {
		LOG_ERR("allocation failed");
		return NULL;
	}

	ret->func = func ? func : hash_default;
	ret->elem_size = elem_size;
	ret->alloc_count = 0;
	ret->max_allocs = count; // we use the size requested by user

	return (hashtable_t*)ret;
}

bool hashtable_insert(hashtable_t* ht, string key, void* elem) {
	if (!ht || !key.len || !elem) {
		LOG_ERR("invalid parameters");
		return false;
	}

	ht_impl* h = (ht_impl*)ht;
	if (h->alloc_count == h->max_allocs) {
		LOG_ERR("hashtable is full");
		return false;
	}

	size_t index = h->func(&key, h->max_allocs);
	void* addr = (char*)h->memory + (index * h->elem_size);
	if (!_is_elem_null(addr, h->elem_size)) {
		LOG_ERR("collision");
		return false;
	}
	memcpy(addr, elem, h->elem_size);

	h->alloc_count++;

	return true;
}

LIB_EXPORT bool hashtable_get(hashtable_t* ht, void* out, string key) {
	if (!ht || !key.len || !out) {
		LOG_ERR("invalid parameters");
		return false;
	}

	ht_impl* h = (ht_impl*)ht;
	if (h->alloc_count == 0) {
		LOG_ERR("hashtable is empty");
		return false;
	}

	size_t index = h->func(&key, h->max_allocs);
	void* addr = (char*)h->memory + (index * h->elem_size);

	if (_is_elem_null(addr, h->elem_size)) {
		LOG_ERR("no data for key");
		return false;
	}

	memcpy(out, addr, h->elem_size);

	return true;
}

uint32_t hashtable_usage(hashtable_t* ht) {
	if (!ht) {
		LOG_ERR("invalid parameter");
		return 0;
	}
	return ((ht_impl*)ht)->alloc_count;
}

void hashtable_reset(hashtable_t* ht) {
	if (!ht) {
		LOG_ERR("invalid parameter");
		return;
	}

	ht_impl* h = (ht_impl*)ht;
	size_t allocation_size = ht_allocation_size(h->max_allocs, h->elem_size);
	memset(h->memory, 0, allocation_size);
	h->alloc_count = 0;
}

// Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp
static uint32_t MurmurOAAT_32(string* key, uint32_t seed) {
	for (size_t i = 0; i < key->len; ++i) {
		seed ^= key->str[i];
		seed *= 0x5bd1e995;
		seed ^= seed >> 15;
	}
	return seed;
}

size_t hash_default(string* key, uint32_t elem_count) {
	if (!key || key->len == 0) {
		LOG_ERR("hashtable_default invalid parameters");
		return 0;
	}
	static uint32_t seed = 0x12345678;

	return MurmurOAAT_32(key, seed) % elem_count;
}

typedef struct ht_iterator_impl {
	uint32_t index;
	ht_impl* table;
} ht_iterator_impl;

hashtable_iterator_t* hashtable_iterate(hashtable_t* table, arena_t* arena) {
	ht_impl* ht = (ht_impl*)table;
	if (!arena || !table) { return NULL; }

	ht_iterator_impl* ret = arena_push_z(arena, ht_iterator_impl);
	if (!ret) {
		LOG_ERR("couldn't allocate iterator");
		return NULL;
	}

	ret->table = ht;
	ret->index = 0;

	return (hashtable_iterator_t*)ret;
}

LIB_EXPORT bool hashtable_iterator_next(hashtable_t* table,
										hashtable_iterator_t* iterator,
										void* out) {
	ht_iterator_impl* it = (ht_iterator_impl*)iterator;
	if (!it || it->table != (ht_impl*)table) { return false; }

	ht_impl* tbl = it->table;

	if (it->index > tbl->max_allocs) { return false; }

	while (it->index < tbl->max_allocs) {
		void* ptr = (char*)tbl->memory + (it->index * tbl->elem_size);
		it->index++;
		if (!_is_elem_null(ptr, tbl->elem_size)) {
			memcpy(out, ptr, tbl->elem_size);
			return true;
		}
	}

	return false;
}
