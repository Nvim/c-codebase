#include <containers/arena.h>
#include "unity.h"
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

void test_arena_create_aligned(void) {
	arena_t* arena = arena_create_aligned(1024, 128);
	TEST_ASSERT_NOT_NULL(arena);

	void* ptr = arena_alloc(arena, 1);
	TEST_ASSERT_NOT_NULL(ptr);
	TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % 128);

	ptr = arena_alloc(arena, 24);
	TEST_ASSERT_NOT_NULL(ptr);
	TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % 128);

	ptr = arena_alloc(arena, 111);
	TEST_ASSERT_NOT_NULL(ptr);
	TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % 128);

	arena_destroy(arena);
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
}

void test_arena_checkpoint_begin(void) {
	arena_t* arena = arena_create(32);
	arena_checkpoint* check = arena_get_checkpoint(arena);

	arena_impl* impl = (arena_impl*)arena;
	TEST_ASSERT_EQUAL(impl->begin, check);

	void* cur = impl->current;
	arena_restore_checkpoint(arena, check, false);
	TEST_ASSERT_EQUAL(impl->current, cur);
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
}

void test_arena_checkpoint_reset_zero(void) {
	arena_t* arena = arena_create(32);
	int* arr = arena_alloc_array(arena, sizeof(int), 4);

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
	arena_restore_checkpoint(arena, (arena_checkpoint*)&arr[0], true);
	TEST_ASSERT_EQUAL(arr[0], 0);
	TEST_ASSERT_EQUAL(arr[1], 0);
	TEST_ASSERT_EQUAL(arr[2], 0);
	TEST_ASSERT_EQUAL(arr[3], 0);
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

	return UNITY_END();
}
