#include "core/platform.h"
#include "unity.h"
#include <containers/arena.h>
#include <string.h>

#include "arena_internal.h"

void setUp(void) {
	// set stuff up here
}

void tearDown(void) {
	// clean stuff up here
}

/* ------------------------------------------------------------------------- */

void test_arena_create_and_destroy(void) {
	arena_t* arena = arena_create(1024);
	TEST_ASSERT_NOT_NULL(arena);

	arena_destroy(arena);
}

/* Tests that beginning pointer is always aligned, even
 * in the case when there is a gap between end of header and beginning of usable
 * memory
 */
void test_arena_create_aligned(void) {

	static size_t const arena_alignments[] = {8, 16, 32, 128, 256, 512};
	for (size_t i = 0; i < 6; ++i) {
		size_t arena_alignment = arena_alignments[i];
		TEST_ASSERT_NOT_EQUAL(0, align_up(ARENA_HEADER_SIZE, arena_alignment));
		if (arena_alignment < ARENA_HEADER_SIZE) {
			TEST_ASSERT_EQUAL(arena_alignment % ARENA_HEADER_SIZE,
							  arena_alignment);
		} else {
			TEST_ASSERT_EQUAL(arena_alignment % ARENA_HEADER_SIZE, 0);
		}

		arena_t* arena = arena_create_aligned(2048, arena_alignment);
		arena_impl* impl = (arena_impl*)arena;
		TEST_ASSERT_NOT_NULL(impl);

		// Test begin position
		if (arena_alignment < ARENA_HEADER_SIZE) {
			// begin should be right after header
			TEST_ASSERT_EQUAL((size_t)impl->begin,
							  (size_t)impl + ARENA_HEADER_SIZE);
		} else {
			// begin should be right after 1st alignment
			TEST_ASSERT_EQUAL((size_t)impl->begin,
							  (size_t)impl + arena_alignment);
		}
		TEST_ASSERT_EQUAL(arena_alignment, impl->alignment);

		// Test some allocs
		void* ptr = arena_alloc(arena, 1);
		TEST_ASSERT_NOT_NULL(ptr);
		TEST_ASSERT_EQUAL(impl->begin, ptr);
		TEST_ASSERT_EQUAL(0, ((uintptr_t)ptr) % arena_alignment);

		ptr = arena_alloc(arena, 24);
		TEST_ASSERT_NOT_NULL(ptr);
		TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % arena_alignment);

		ptr = arena_alloc(arena, 111);
		TEST_ASSERT_NOT_NULL(ptr);
		TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % arena_alignment);
		arena_destroy(arena);
	}
}

void test_arena_alloc(void) {
	arena_t* arena = arena_create(128);

	void* p1 = arena_alloc(arena, 16);
	void* p2 = arena_alloc(arena, 16);

	TEST_ASSERT_NOT_NULL(p1);
	TEST_ASSERT_NOT_NULL(p2);
	TEST_ASSERT_NOT_EQUAL(p1, p2);
	TEST_ASSERT_EQUAL((char*)p1 + 16, p2);

	arena_destroy(arena);
}

void test_arena_alloc_too_much(void) {
	arena_t* arena = arena_create(256);
	void* p = arena_alloc(arena, 256);
	TEST_ASSERT_NOT_NULL(p);

	void* fails = arena_alloc(arena, 1);
	TEST_ASSERT_NULL(fails);
	arena_destroy(arena);
}

void test_arena_alloc_array(void) {
	arena_t* arena = arena_create(256);

	int* array = (int*)arena_alloc_array(arena, sizeof(int), 10);
	TEST_ASSERT_NOT_NULL(array);

	for (int i = 0; i < 10; ++i) {
		array[i] = i;
		TEST_ASSERT_EQUAL_INT(i, array[i]);
	}

	arena_destroy(arena);
}

void test_arena_get_usage_and_remaining(void) {
	arena_t* arena = arena_create(96);

	TEST_ASSERT_EQUAL_size_t(0, arena_get_usage(arena));
	TEST_ASSERT_EQUAL_size_t(96, arena_get_remaining(arena));

	arena_alloc(arena, 48);

	TEST_ASSERT_EQUAL_size_t(48, arena_get_usage(arena));
	TEST_ASSERT_EQUAL_size_t(48, arena_get_remaining(arena));

	arena_destroy(arena);
}

/*
 * [NOTE]: Assumes 8 bytes alignment.
 * 100 -> 104
 * 50 -> 56
 */
void test_arena_get_usage_and_remaining_misaligned(void) {
	arena_t* arena = arena_create(100);

	TEST_ASSERT_EQUAL_size_t(0, arena_get_usage(arena));
	TEST_ASSERT_EQUAL_size_t(104, arena_get_remaining(arena));

	arena_alloc(arena, 50);

	TEST_ASSERT_EQUAL_size_t(56, arena_get_usage(arena));
	TEST_ASSERT_EQUAL_size_t(104 - 56, arena_get_remaining(arena));

	arena_destroy(arena);
}

void test_arena_reset(void) {
	arena_t* arena = arena_create(64);
	arena_impl* impl = (arena_impl*)arena;
	void* begin = impl->begin;

	void* p1 = arena_alloc(arena, 32);
	TEST_ASSERT_NOT_NULL(p1);
	TEST_ASSERT_EQUAL_size_t(32, arena_get_usage(arena));

	void* base = arena_reset(arena);
	TEST_ASSERT_NOT_NULL(base);
	TEST_ASSERT_EQUAL_size_t(0, arena_get_usage(arena));
	TEST_ASSERT_EQUAL_size_t(64, arena_get_remaining(arena));
	TEST_ASSERT_EQUAL(begin, impl->current);

	void* p2 = arena_alloc(arena, 64);
	TEST_ASSERT_NOT_NULL(p2);
	TEST_ASSERT_EQUAL_size_t(64, arena_get_usage(arena));
	TEST_ASSERT_EQUAL_size_t(0, arena_get_remaining(arena));

	arena_destroy(arena);
}

void test_arena_reset_zero(void) {
	arena_t* arena = arena_create(64);

	uint8_t* p = (uint8_t*)arena_alloc(arena, 16);
	TEST_ASSERT_NOT_NULL(p);

	memset(p, 0xFF, 16);

	arena_reset_zero(arena);

	uint8_t* p2 = (uint8_t*)arena_alloc(arena, 16);
	for (int i = 0; i < 16; ++i) { TEST_ASSERT_EQUAL_UINT8(0, p2[i]); }

	arena_destroy(arena);
}

void test_arena_checkpoint(void) {
	arena_t* arena = arena_create(64);

	uint8_t* p = arena_alloc(arena, 16);
	TEST_ASSERT_NOT_NULL(p);

	arena_checkpoint* check = arena_get_checkpoint(arena);
	TEST_ASSERT_NOT_NULL(p);

	uint8_t* p2 = arena_alloc(arena, 12);
	TEST_ASSERT_NOT_NULL(p2);
	TEST_ASSERT_EQUAL((void*)p2, (void*)check);

	arena_restore_checkpoint(arena, check, false);
	uint8_t* p3 = arena_alloc(arena, 14);
	TEST_ASSERT_EQUAL(p2, p3);
	arena_destroy(arena);
}

void test_arena_checkpoint_begin(void) {
	arena_t* arena = arena_create(32);
	arena_checkpoint* check = arena_get_checkpoint(arena);

	arena_impl* impl = (arena_impl*)arena;
	TEST_ASSERT_EQUAL(impl->begin, check);

	void* cur = impl->current;
	arena_restore_checkpoint(arena, check, false);
	TEST_ASSERT_EQUAL(impl->current, cur);
	arena_destroy(arena);
}

void test_arena_checkpoint_restore_invalid(void) {
	arena_t* arena = arena_create(32);
	arena_impl* impl = (arena_impl*)arena;

	arena_alloc(arena, 8);
	arena_alloc(arena, 8);

	arena_checkpoint* check = arena_get_checkpoint(arena);

	arena_reset(arena);
	void* cur = impl->current;
	arena_restore_checkpoint(arena, check, false);
	TEST_ASSERT_EQUAL(impl->current, cur);
	arena_destroy(arena);
}

void test_arena_checkpoint_reset_zero(void) {
	arena_t* arena = arena_create(32);
	int* arr = arena_alloc_array(arena, sizeof(int), 4);
	arena_impl* impl = (arena_impl*)arena;
	TEST_ASSERT_EQUAL_PTR(impl->begin, arr);

	void* check;
	for (int i = 0; i < 4; i++) {
		arr[i] = i;
		if (i == 2) { check = arena_get_checkpoint(arena); }
	}

	// Data should still be there:
	arena_restore_checkpoint(arena, check, false);
	TEST_ASSERT_EQUAL(arr[2], 2);
	TEST_ASSERT_EQUAL(arr[3], 3);

	// Everything is wiped:
	arena_restore_checkpoint(arena, (arena_checkpoint*)arr, true);
	TEST_ASSERT_EQUAL(arr[0], 0);
	TEST_ASSERT_EQUAL(arr[1], 0);
	TEST_ASSERT_EQUAL(arr[2], 0);
	TEST_ASSERT_EQUAL(arr[3], 0);
	arena_destroy(arena);
}

typedef struct sixtyfour {
	size_t a[8];
} sixtyfour;
_Static_assert(sizeof(sixtyfour) == 64, "");

// [NOTE]: Assumes 4096 page size
void test_arena_growing(void) {
	arena_create_info info = {
		.alignment = 16,
		.initial_size = 2048,
		.max_size = 16384,
		.flags = ARENA_GROWING | ARENA_LOG,
	};
	arena_t* arena = arena_create_from_config(info);
	TEST_ASSERT_NOT_NULL(arena);

	arena_impl* impl = (arena_impl*)arena;
	TEST_ASSERT_EQUAL(2048, impl->allocation_size); // Committed whole page
	TEST_ASSERT_EQUAL(16384, impl->max_size);

	// No gap between header and 1st alloc:
	TEST_ASSERT_EQUAL((size_t)impl + ARENA_HEADER_SIZE, (size_t)impl->begin);

	// Alloc 2048. Should be full, but not grown yet
	sixtyfour* arr = arena_alloc_array(arena, 64, 32);
	TEST_ASSERT_NOT_NULL(arr);
	TEST_ASSERT_EQUAL(2048, impl->allocation_size);
	TEST_ASSERT_EQUAL(0, arena_get_remaining(arena));
	TEST_ASSERT_EQUAL(2048, arena_get_usage(arena));

	// Alloc some more. Should grow to match committed size:
	// [64][2048][1984] = 4096 total
	sixtyfour* arr2 = arena_alloc_array(arena, 64, 31);
	TEST_ASSERT_NOT_NULL(arr2);
	TEST_ASSERT_EQUAL(4096 - 64, impl->allocation_size);
	TEST_ASSERT_EQUAL(0, arena_get_remaining(arena));
	TEST_ASSERT_EQUAL(4096 - 64, arena_get_usage(arena));

	// [TODO]: commit more
}

/* ------------------------------------------------------------------------- */

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_arena_create_and_destroy);
	RUN_TEST(test_arena_create_aligned);
	RUN_TEST(test_arena_alloc);
	RUN_TEST(test_arena_alloc_too_much);
	RUN_TEST(test_arena_alloc_array);
	RUN_TEST(test_arena_get_usage_and_remaining_misaligned);
	RUN_TEST(test_arena_get_usage_and_remaining);
	RUN_TEST(test_arena_reset);
	RUN_TEST(test_arena_reset_zero);
	RUN_TEST(test_arena_checkpoint);
	RUN_TEST(test_arena_checkpoint_begin);
	RUN_TEST(test_arena_checkpoint_restore_invalid);
	RUN_TEST(test_arena_checkpoint_reset_zero);
	RUN_TEST(test_arena_growing);

	return UNITY_END();
}
