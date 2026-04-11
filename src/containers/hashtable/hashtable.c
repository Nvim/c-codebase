#include "hashtable_internal.h"

#include <allocators/arena.h>
#include <core/log.h>
#include <strings/string.h>

#include <string.h>

static bool _is_elem_null(void* elem, uint32_t elem_size) {
	for (uint32_t i = 0; i < elem_size; i++) {
		if (((char*)elem)[i] != 0x0) { return false; }
	}
	return true;
}

hashtable_t* hashtable_create(arena_t* arena, uint32_t count,
							  uint32_t elem_size, hash_fn func) {
	if (!arena || count == 0 || elem_size == 0) {
		LOG_ERR("hashtable_create: invalid parameters");
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
		LOG_ERR("hashtable_create: allocation failed");
		return NULL;
	}

	ret->memory = arena_alloc(arena, allocation_size);
	if (!ret->memory) {
		LOG_ERR("hashtable_create: allocation failed");
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
		LOG_ERR("hashtable_insert: invalid parameters");
		return false;
	}

	ht_impl* h = (ht_impl*)ht;
	if (h->alloc_count == h->max_allocs) {
		LOG_ERR("hashtable_insert: hashtable is full");
		return false;
	}

	size_t index = h->func(&key, h->max_allocs);
	void* addr = (char*)h->memory + (index * h->elem_size);
	if (!_is_elem_null(addr, h->elem_size)) {
		LOG_ERR("hashtable_insert: collision");
		return false;
	}
	memcpy(addr, elem, h->elem_size);

	h->alloc_count++;

	return true;
}

LIB_EXPORT bool hashtable_get(hashtable_t* ht, void* out, string key) {
	if (!ht || !key.len || !out) {
		LOG_ERR("hashtable_get: invalid parameters");
		return false;
	}

	ht_impl* h = (ht_impl*)ht;
	if (h->alloc_count == 0) {
		LOG_ERR("hashtable_get: hashtable is empty");
		return false;
	}

	size_t index = h->func(&key, h->max_allocs);
	void* addr = (char*)h->memory + (index * h->elem_size);

	if (_is_elem_null(addr, h->elem_size)) {
		LOG_ERR("hashtable_insert: no data for key");
		return false;
	}

	memcpy(out, addr, h->elem_size);

	return true;
}

uint32_t hashtable_usage(hashtable_t* ht) {
	if (!ht) {
		LOG_ERR("hashtable_reset: invalid parameter");
		return 0;
	}
	return ((ht_impl*)ht)->alloc_count;
}

LIB_EXPORT void hashtable_reset(hashtable_t* ht) {
	if (!ht) {
		LOG_ERR("hashtable_reset: invalid parameter");
		return;
	}

	ht_impl* h = (ht_impl*)ht;
	size_t allocation_size = ht_allocation_size(h->max_allocs, h->elem_size);
	memset(h->memory, 0, allocation_size);
	h->alloc_count = 0;
}

// Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp
uint32_t MurmurOAAT_32(string* key, uint32_t seed) {
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
