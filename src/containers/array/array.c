#include "array_internal.h"

#include <allocators/arena.h>
#include <containers/array.h>
#include <core/log.h>
#include <string.h>

array_t* _array_create(arena_t* arena, size_t size, size_t elem_size) {
	if (!arena || size == 0 || elem_size == 0) {
		LOG_ERR("array_create: invalid parameters");
		return NULL;
	}

	size_t alloc_size = (elem_size * size) + sizeof(array_impl);

	if (alloc_size > arena_get_growth_remaining(arena)) {
		LOG_ERR("array_create: not enough space in arena");
		return NULL;
	}

	arena_checkpoint* cp = arena_get_checkpoint(arena);
	array_impl* arr = arena_alloc(arena, sizeof(array_impl));
	if (!arr) {
		LOG_ERR("array_create: couldn't allocate array");
		return NULL;
	}
	void* memory = arena_alloc(arena, size * elem_size);
	if (!memory) {
		LOG_ERR("array_create: couldn't allocate array buffer");
		arena_restore_checkpoint(arena, cp, false);
		return NULL;
	}

	arr->elem_size = elem_size;
	arr->size = size;
	arr->memory = memory;
	return (array_t*)arr;
}

size_t _array_size(array_t* array) {
	if (!array) {
		LOG_ERR("array_size: invalid array");
		return 0;
	}
	array_impl* arr = (array_impl*)array;
	return arr->size;
}

size_t _array_stride(array_t* array) {
	if (!array) {
		LOG_ERR("array_stride: invalid array");
		return 0;
	}
	array_impl* arr = (array_impl*)array;
	return arr->elem_size;
}

bool _array_at(array_t* array, void** out, size_t index) {
	if (!array || index >= _array_size(array)) {
		LOG_ERR("array_at: invalid parameters");
		return false;
	}

	array_impl* arr = (array_impl*)array;

	void* addr = (char*)arr->memory + (index * arr->elem_size);
	*out = addr;
	return true;
}

bool _array_at_const(array_t* array, void const** out, size_t index) {
	if (!array || index >= _array_size(array)) {
		LOG_ERR("array_at: invalid parameters");
		return false;
	}

	array_impl* arr = (array_impl*)array;

	void* const addr = (char*)arr->memory + (index * arr->elem_size);
	*out = addr;
	return true;
}

bool _array_get(array_t* array, void* out, size_t index) {
	if (!array || !out || index >= _array_size(array)) {
		LOG_ERR("array_at: invalid parameters");
		return false;
	}

	array_impl* arr = (array_impl*)array;

	void* addr = (char*)arr->memory + (index * arr->elem_size);
	memcpy(out, addr, arr->elem_size);
	return true;
}

bool _array_set(array_t* array, void* value, size_t index) {
	if (!array || index >= _array_size(array)) {
		LOG_ERR("array_set: invalid parameters");
		return false;
	}

	array_impl* arr = (array_impl*)array;

	void* addr = (char*)arr->memory + (index * arr->elem_size);
	memcpy(addr, value, arr->elem_size);

	return true;
}

void _array_clear(array_t* array) {
	if (!array) {
		LOG_ERR("array_clear: invalid parameters");
		return;
	}

	array_impl* arr = (array_impl*)array;
	memset(arr->memory, 0, arr->size * arr->elem_size);
}
