#include "arena_internal.h"

#include <allocators/arena.h>
#include <common.h>
#include <platform/platform.h>

#include <string.h>
#include <unity.h>

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
	TEST_PASS_MESSAGE("Assertions will crash the test");
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
	TEST_ASSERT_EQUAL(16384, arena_get_growth_remaining(arena));

	arena_impl* impl = (arena_impl*)arena;
	TEST_ASSERT_EQUAL(2048, impl->allocation_size); // Committed whole page
	TEST_ASSERT_EQUAL(16384, impl->max_size);

	// No gap between header and 1st alloc:
	TEST_ASSERT_EQUAL((size_t)impl + ARENA_HEADER_SIZE, (size_t)impl->begin);

	{ // Alloc 2048. Should be full, but not grown yet
		sixtyfour* arr = arena_alloc_array(arena, 64, 32); // 2048 bytes
		TEST_ASSERT_NOT_NULL(arr);
		TEST_ASSERT_EQUAL(2048, impl->allocation_size);
		TEST_ASSERT_EQUAL(0, arena_get_remaining(arena));
		TEST_ASSERT_EQUAL(2048, arena_get_usage(arena));
		TEST_ASSERT_EQUAL((size_t)impl->begin + 2040, (size_t)&arr[31].a[7]);
		TEST_ASSERT_EQUAL(16384 - 2048, arena_get_growth_remaining(arena));
	}

	// Alloc some more. Should grow to match committed size:
	{ // [64][2048][1984] = 4096 total
		sixtyfour* arr2 = arena_alloc_array(arena, 64, 31); // 1984 bytes
		TEST_ASSERT_NOT_NULL(arr2);
		TEST_ASSERT_EQUAL(4096 - ARENA_HEADER_SIZE, impl->allocation_size);
		TEST_ASSERT_EQUAL(0, arena_get_remaining(arena));
		TEST_ASSERT_EQUAL(4096 - ARENA_HEADER_SIZE, arena_get_usage(arena));

		// Try write last address:
		size_t* last_addr = &arr2[30].a[7];
		*last_addr = 71;
		TEST_ASSERT_EQUAL(71, *last_addr);
		TEST_ASSERT_EQUAL((size_t)impl->begin + 4024, (size_t)&arr2[30].a[7]);
		TEST_ASSERT_EQUAL(16384 - 2048 - 1984,
						  arena_get_growth_remaining(arena));
	}

	// Commit one more page
	{
		sixtyfour* arr3 = arena_alloc_array(arena, 64, 32); // 2048 bytes again
		TEST_ASSERT_NOT_NULL(arr3);
		TEST_ASSERT_EQUAL(8192 - ARENA_HEADER_SIZE, impl->allocation_size);
		TEST_ASSERT_EQUAL(2048, arena_get_remaining(arena));
		TEST_ASSERT_EQUAL(4096 + 2048 - ARENA_HEADER_SIZE,
						  arena_get_usage(arena));

		// Try writing:
		size_t* last_addr = &arr3[31].a[7];
		*last_addr = 24;
		TEST_ASSERT_EQUAL(24, *last_addr);

		size_t last_elem_off = 2048 + 1984 + 2048; // 3 allocs
		last_elem_off -= 64;					   // 1 sixtyfour
		last_elem_off += 8 * 7;					   // 7 size_t
		TEST_ASSERT_EQUAL((size_t)impl->begin + last_elem_off,
						  (size_t)&arr3[31].a[7]);
		TEST_ASSERT_EQUAL(16384 - 2048 - 1984 - 2048,
						  arena_get_growth_remaining(arena));
	}
	arena_destroy(arena);
}

void test_arena_grow_non_maxed_current(void) {
	// [TODO]: try to make an alloc that will trigger growing
	// even though current isn't at the very end
	// (used: 4000, alloc_size: 4096 -> try alloc 120)
	TEST_ASSERT_TRUE(true);
}

void test_arena_grow_too_much(void) {
	arena_create_info info = {
		.alignment = 16,
		.initial_size = 4096 - ARENA_HEADER_SIZE,
		.max_size = 16384,
		.flags = ARENA_GROWING | ARENA_LOG,
	};
	arena_t* arena = arena_create_from_config(info);

	arena_impl* impl = (arena_impl*)arena;
	TEST_ASSERT_TRUE(
		((size_t)impl + impl->allocation_size + ARENA_HEADER_SIZE) % 4096 == 0);

	{ // 1st alloc to fill page
		size_t prev = impl->allocation_size;
		void* ptr = arena_alloc(arena, 4096 - ARENA_HEADER_SIZE);
		TEST_ASSERT_NOT_NULL(ptr);
		TEST_ASSERT_EQUAL(prev, impl->allocation_size); // shouldn't grow
	}

	for (int i = 0; i < 3; i++) {
		void* ptr = arena_alloc(arena, 4096);
		TEST_ASSERT_NOT_NULL(ptr);
		memset(ptr, i * 10, 4096);
		TEST_ASSERT_EQUAL(i * 10, ((char*)ptr)[0]);
		TEST_ASSERT_EQUAL(i * 10, ((char*)ptr)[4095]);
	}

	// 64 free bytes left because of header:
	TEST_ASSERT_EQUAL(impl->max_size - ARENA_HEADER_SIZE,
					  impl->allocation_size);
	TEST_ASSERT_EQUAL(16320, arena_get_usage(arena));

	// They are on a separate page:
	// [64][4032][4096][4096][4096][FREE 64]
	// ^         ^     ^     ^     ^
	TEST_ASSERT_TRUE((size_t)impl->current % 4096 == 0);
	TEST_ASSERT_EQUAL(
		0, arena_get_remaining(arena)); // amount before next "growth"

	{ // Could extend to page end, but hard cap at max_size
		size_t prev = impl->allocation_size;
		size_t* ptr = NULL;
		// TODO: Assert compat somehow
#if false
		ptr = arena_alloc(arena, 65);
		TEST_ASSERT_NULL(ptr);
		TEST_ASSERT_EQUAL(prev, impl->allocation_size);
#endif

		// We still have these 64 bytes though
		ptr = arena_alloc(arena, 32);
		TEST_ASSERT_NOT_NULL(ptr);
		TEST_ASSERT_EQUAL(prev + 64, impl->allocation_size);
		ptr[3] = 823;
		TEST_ASSERT_EQUAL(823, ptr[3]);

		ptr = arena_alloc(arena, 32);
		TEST_ASSERT_NOT_NULL(ptr);
		TEST_ASSERT_EQUAL(prev + 64, impl->allocation_size);
		ptr[3] = 237;
		TEST_ASSERT_EQUAL(237, ptr[3]);
	}
	arena_destroy(arena);
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
	RUN_TEST(test_arena_grow_too_much);

	return UNITY_END();
}
