#include "arena_internal.h"
#include <containers/arena.h>
#include <core/log.h>
#include <core/platform.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG(impl, msg, ...)                                                  \
	{                                                                          \
		if (impl->flags & ARENA_LOG) { LOG_DEBUG(msg, ##__VA_ARGS__); }        \
	}

static void extend_to_page_end(arena_impl* impl);
static bool commit_more(arena_impl* impl, size_t size, bool is_page_full);

static arena_impl* _arena_create_internal(arena_create_info info) {
	if (!info.alignment) {
		LOG_ERR("arena_create: bad alignment");
		return NULL;
	}

	bool const growable = info.flags & ARENA_GROWING;
	if (growable) {
		if (info.max_size == 0 || info.max_size < info.alignment) {
			LOG_ERR("arena_create: invalid max size");
			return NULL;
		}
	} else {
		if (info.initial_size == 0) {
			LOG_ERR("arena_create: invalid initial size");
			return NULL;
		}
	}

	size_t const header_size_align =
		align_up(ARENA_HEADER_SIZE, info.alignment);
	size_t const size_align = align_up(info.initial_size, info.alignment);
	size_t const max_size_align = align_up(info.max_size, info.alignment);
	size_t const commit_size =
		align_up(size_align + header_size_align, platform_memory_page_size());
	void* memory;

	if (!growable) {
		/**
		 * [TODO]: Non-growing arena doesn't need to be page-aligned.
		 * Page-aligned guarantees that `begin` will always be right after
		 * `header_size_align`, which makes computing offsets easier since gap
		 * between header and begin is always identical
		 */

		// memory = platform_memory_alloc(size_align + header_size_align);
		// if (!memory) {
		// 	LOG_ERR("arena_create: arena allocation failed");
		// 	return NULL;
		// }
		memory = platform_memory_reserve(commit_size);
		if (!memory) {
			LOG_ERR(
				"arena_create: arena allocation for %zu (%zu requested) failed",
				commit_size, info.initial_size);
			return NULL;
		}

		if (!platform_memory_commit(memory, commit_size)) {
			LOG_ERR("arena_create: couldn't commit memory");
			if (!platform_memory_dereserve(memory, commit_size)) {
				LOG_ERR("arena_create: couldn't de-reserve memory");
			}
			return NULL;
		}
	} else {
		size_t const reserve_size = max_size_align + header_size_align;

		memory = platform_memory_reserve(reserve_size);
		if (!memory) {
			LOG_ERR("arena_create: arena allocation failed");
			return NULL;
		}

		if (!platform_memory_commit(memory, commit_size)) {
			LOG_ERR("arena_create: couldn't commit memory");
			if (!platform_memory_dereserve(memory, reserve_size)) {
				LOG_ERR("arena_create: couldn't de-reserve memory");
			}
			return NULL;
		}
		LOG_INFO("Creating growing arena (%zu reserve, %zu initial commit)",
				 reserve_size, commit_size);
	}

	size_t begin = (size_t)memory + header_size_align;
	arena_impl* impl = memory;
	{
		impl->flags = info.flags;
		impl->alignment = info.alignment;
		impl->max_size = (growable) ? info.max_size : 0;
		impl->allocation_size = size_align;
		impl->begin = (void*)begin;
		impl->current = impl->begin;
	}

	if (impl->flags & ARENA_ZERO_ALWAYS) {
		memset(impl->begin, 0, impl->allocation_size);
	}

	DEBUG(impl, "arena_create: committed: %zu (%zu requested)", size_align,
		  info.initial_size);
	DEBUG(impl, "arena_create: memory chunk is at %p, begin is at %p", memory,
		  impl->begin);
	return impl;
}

arena_t* arena_create(size_t size) {
	arena_create_info info;
	memset(&info, 0, sizeof(info));
	info.initial_size = size;
	info.alignment = ALIGNMENT;
	return (arena_t*)_arena_create_internal(info);
}

arena_t* arena_create_aligned(size_t size, size_t alignment) {
	arena_create_info info;
	memset(&info, 0, sizeof(info));
	info.initial_size = size;
	info.alignment = alignment;
	info.flags |= ARENA_LOG;
	return (arena_t*)_arena_create_internal(info);
}

arena_t* arena_create_from_config(arena_create_info info) {
	if (info.flags & ARENA_EXTERNAL_MEMORY) {
		LOG_ERR("arena_create_from_config: invalid flags");
		return NULL;
	}
	return (arena_t*)_arena_create_internal(info);
}

arena_t* arena_create_from_memory(void* memory, arena_create_info info) {
	if (!memory) {
		LOG_ERR("arena_create_from: invalid memory");
		return NULL;
	}

	if (info.flags & ARENA_GROWING) {
		LOG_ERR("arena_create_from_memory: invalid flags");
		return NULL;
	}

	size_t size = info.initial_size;
	uint8_t alignment = info.alignment;

	if (size <= ARENA_HEADER_SIZE || size % alignment != 0) {
		LOG_ERR("arena_create_from: memory is too short or not aligned");
		return NULL;
	}

	size_t begin = (size_t)memory + ARENA_HEADER_SIZE;
	begin = align_up(begin, info.alignment);
	arena_impl* impl = memory;
	{
		impl->begin = (void*)begin;
		impl->current = impl->begin;
		impl->allocation_size = size - ARENA_HEADER_SIZE;
		impl->alignment = alignment;
		impl->flags = info.flags | ARENA_EXTERNAL_MEMORY;
	}

	return (arena_t*)impl;
}

size_t arena_header_size(void) { return ARENA_HEADER_SIZE; }

void* arena_alloc(arena_t* arena, size_t size) {
	if (!arena) {
		LOG_ERR("arena_alloc: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	size = align_up(size, impl->alignment);
	DEBUG(impl, "arena_alloc: allocating %zu", size);

	size_t remaining = arena_impl_remaining(impl);
	if (remaining < size) {
		if (!(impl->flags & ARENA_GROWING)) {
			LOG_ERR("arena_alloc: not enough space for %zu. (%zu left)", size,
					arena_impl_remaining(impl));
			return NULL;
		}

		if (arena_get_usage(arena) + size > impl->max_size) {
			LOG_ERR("not enough space in growable arena for %zu.", size);
			return NULL;
		}

		bool const is_page_full =
			(size_t)impl->current % platform_memory_page_size() == 0;

		// First, extend to current page's end
		if (!is_page_full) { extend_to_page_end(impl); }

		remaining = arena_impl_remaining(impl);
		if (remaining < size) {
			// Commit more pages:
			if (!commit_more(impl, size, is_page_full)) {
				LOG_ERR("arena_grow: couldn't commit more pages");
				return NULL;
			}
		}
	}

	void* ret = impl->current;
	impl->current = (char*)impl->current + size;
	return ret;
}

void* arena_alloc_array(arena_t* arena, size_t elem_size, size_t elem_count) {
	if (!arena) {
		LOG_ERR("arena_alloc_array: invalid arena handle: %p", arena);
		return NULL;
	}
	size_t size = elem_size * elem_count;
	return arena_alloc(arena, size);
}

void* arena_reset(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_reset: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	impl->current = impl->begin;
	if (impl->flags & ARENA_ZERO_ALWAYS) {
		memset(impl->begin, 0, impl->allocation_size);
	}
	return impl->current;
}

void* arena_reset_zero(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_reset_zero: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	impl->current = impl->begin;
	memset(impl->begin, 0, impl->allocation_size);

	return impl->current;
}

void arena_destroy(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_destroy: invalid arena handle: %p", arena);
		return;
	}
	arena_impl* impl = (arena_impl*)arena;

	if (impl->flags & ARENA_EXTERNAL_MEMORY) {
		LOG_WARN("arena_destroy: called on externally backed arena. Ignoring.",
				 arena);
		return;
	}

	if (!impl->begin || !impl->allocation_size) {
		LOG_WARN("arena_destroy: arena has no allocation");
		return;
	}

	if (impl->flags & ARENA_ZERO_ALWAYS) {
		memset(impl->begin, 0, impl->allocation_size);
	}

	size_t const header_size_align =
		align_up(ARENA_HEADER_SIZE, impl->alignment);
	if (impl->flags & ARENA_GROWING) {
		assert(arena == platform_memory_page_begin(arena));

		// size_t const to_decommit =
		// 	align_up(impl->allocation_size + header_size_align,
		// 			 platform_memory_page_size());
		size_t const to_dereserve = impl->max_size + header_size_align;

		// if (!platform_memory_decommit(arena, to_decommit)) {
		// 	LOG_ERR("arena_destroy: couldn't decommit arena memory");
		// }
		if (!platform_memory_dereserve(arena, to_dereserve)) {
			LOG_ERR("arena_destroy: couldn't dereserve arena memory");
		}
	} else {
		assert(arena == platform_memory_page_begin(arena));
		size_t const to_dereserve = impl->allocation_size + header_size_align;
		if (!platform_memory_dereserve(arena, to_dereserve)) {
			LOG_ERR("arena_destroy: couldn't dereserve arena memory");
		}
		// platform_memory_free(arena);
	}
}

size_t arena_get_usage(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_get_usage: invalid arena handle: %p", arena);
		return 0;
	}
	arena_impl* impl = (arena_impl*)arena;

	size_t used = (size_t)impl->current - (size_t)impl->begin;
	return used;
}

size_t arena_get_remaining(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_get_remaining: invalid arena handle: %p", arena);
		return 0;
	}
	arena_impl* impl = (arena_impl*)arena;

	size_t remaining = arena_impl_remaining(impl);
	return remaining;
}

arena_checkpoint* arena_get_checkpoint(arena_t* arena) {
	if (!arena) {
		LOG_ERR("arena_get_checkpoint: invalid arena handle: %p", arena);
		return NULL;
	}
	arena_impl* impl = (arena_impl*)arena;

	DEBUG(impl, "setting checkpoint at %p.", impl->current);
	return impl->current;
}

void arena_restore_checkpoint(arena_t* arena, arena_checkpoint* checkpoint,
							  bool zero) {
	if (!arena) {
		LOG_ERR("arena_restore_checkpoint: invalid arena handle: %p", arena);
		return;
	}
	arena_impl* impl = (arena_impl*)arena;
	void* p = checkpoint;

	if (impl->current == p) { return; }

	if ((char*)impl->current < (char*)p) {
		LOG_ERR("arena_restore_checkpoint: invalid checkpoint: %p", checkpoint);
		return;
	}

	size_t offset = (char*)impl->current - (char*)p;
	impl->current = p;
	if (zero || impl->flags & ARENA_ZERO_ALWAYS) { memset(p, 0, offset); }
}

static void extend_to_page_end(arena_impl* impl) {
	size_t commit_boundary = (size_t)platform_memory_next_page(impl->current);
	size_t extended_size = commit_boundary - (size_t)impl->begin;
	if (extended_size <= impl->max_size) {
		impl->allocation_size = commit_boundary - (size_t)impl->begin;
		DEBUG(impl,
			  "arena_alloc: extended allocation to page end (new size: "
			  "%zu)",
			  impl->allocation_size);
	} else {
		impl->allocation_size = impl->max_size;
		DEBUG(impl,
			  "arena_alloc: extended allocation to max_size (new size: "
			  "%zu)",
			  impl->allocation_size);
	}
}

static bool commit_more(arena_impl* impl, size_t size, bool is_page_full) {
	size_t remaining = arena_impl_remaining(impl);
	size_t const needed = size - remaining;
	size_t const reserved = impl->max_size - impl->allocation_size;
	DEBUG(impl, "arena_alloc: %zu remaining. Needed: %zu", remaining, needed);

	if (needed > reserved) {
		LOG_ERR("arena_alloc: not enough space in growable arena for "
				"%zu. (%zu left)",
				needed, reserved);
		return false;
	}

	size_t const needed_align = align_up(needed, platform_memory_page_size());

	void* next_page = impl->current;
	if (!is_page_full) { next_page = platform_memory_next_page(impl->current); }

	if (!platform_memory_commit(next_page, needed_align)) {
		LOG_ERR("arena_alloc: couldn't commit %zu bytes out of "
				"growable arena.",
				needed_align);
		return false;
	}
	impl->allocation_size += needed_align;
	if (impl->allocation_size > impl->max_size) {
		impl->allocation_size = impl->max_size;
	}
	DEBUG(impl, "arena_alloc: committed %zu more bytes of reserved space at %p",
		  needed_align, next_page);
	return true;
}
