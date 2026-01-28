#include "arena.h"
#include "unity.h"
#include <string.h>

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
	arena_t* arena = arena_create_aligned(1024, 16);
	TEST_ASSERT_NOT_NULL(arena);

	void* ptr = arena_alloc(arena, 1);
	TEST_ASSERT_NOT_NULL(ptr);
	TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % 16);

	ptr = arena_alloc(arena, 24);
	TEST_ASSERT_NOT_NULL(ptr);
	TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % 16);

	ptr = arena_alloc(arena, 111);
	TEST_ASSERT_NOT_NULL(ptr);
	TEST_ASSERT_EQUAL_UINT(0, ((uintptr_t)ptr) % 16);

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
 * NOTE: Assumes 8 bytes alignment.
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

	void* p1 = arena_alloc(arena, 32);
	TEST_ASSERT_NOT_NULL(p1);
	TEST_ASSERT_EQUAL_size_t(32, arena_get_usage(arena));

	void* base = arena_reset(arena);
	TEST_ASSERT_NOT_NULL(base);
	TEST_ASSERT_EQUAL_size_t(0, arena_get_usage(arena));
	TEST_ASSERT_EQUAL_size_t(64, arena_get_remaining(arena));

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

	return UNITY_END();
}
