#include "hashtable_internal.h"

#include <allocators/arena.h>
#include <containers/hashtable.h>
#include <strings/string.h>
#include <core/platform.h>
#include <unity.h>

void setUp(void) {}

void tearDown(void) {}

void test_hashtable_create(void) {
	arena_t* arena = arena_create(4032);
	TEST_ASSERT_NOT_NULL(arena);

	hashtable_t* ht = hashtable_create(arena, 100, sizeof(int), hash_default);
	TEST_ASSERT_NOT_NULL(ht);
	ht_impl* h = (ht_impl*)ht;
	TEST_ASSERT_EQUAL(0, h->alloc_count);
	TEST_ASSERT_EQUAL(sizeof(int), h->elem_size);
	TEST_ASSERT_EQUAL((char*)arena + arena_header_size() + sizeof(*h),
					  h->memory);
	TEST_ASSERT_EQUAL(hash_default, h->func);
}

void test_hashtable_use_basic(void) {
	string key;
	string key2;
	int elem = 28;
	int elem2 = 42;
	int res1, res2;

	arena_t* arena = arena_create(4032);
	hashtable_t* ht = hashtable_create(arena, 100, sizeof(int), NULL);

	{ // Init
		TEST_ASSERT_NOT_NULL(arena);
		TEST_ASSERT_NOT_NULL(ht);

		TEST_ASSERT_TRUE(string_create(arena, &key, "key_one"));
		TEST_ASSERT_TRUE(string_create(arena, &key2, "key_two"));
	}

	{ // Insert & Get
		TEST_ASSERT_TRUE(hashtable_insert(ht, &key, &elem));
		TEST_ASSERT_TRUE(hashtable_insert(ht, &key2, &elem2));

		TEST_ASSERT_EQUAL(2, hashtable_usage(ht));

		TEST_ASSERT_TRUE(hashtable_get(ht, &res1, &key));
		TEST_ASSERT_TRUE(hashtable_get(ht, &res2, &key2));

		TEST_ASSERT_EQUAL(elem, res1);
		TEST_ASSERT_EQUAL(elem2, res2);
	}

	{ // Get invalid key
		key.str[6] = 'z';
		TEST_ASSERT_FALSE(hashtable_get(ht, &res1, &key));
	}

	{ // Reset
		hashtable_reset(ht);
		TEST_ASSERT_FALSE(hashtable_get(ht, &res2, &key2));
		TEST_ASSERT_EQUAL(0, hashtable_usage(ht));
	}
}

void test_hashtable_allocation_size(void) {
	TEST_ASSERT_EQUAL(.8f, HT_RATIO);

	arena_t* arena = arena_create(4032);
	TEST_ASSERT_NOT_NULL(arena);

	hashtable_t* ht = hashtable_create(arena, 100, sizeof(int), hash_default);
	TEST_ASSERT_NOT_NULL(ht);

	ht_impl* h = (ht_impl*)ht;
	TEST_ASSERT_EQUAL(100, h->max_allocs);
	// 400*1.2 = 500
	TEST_ASSERT_EQUAL(align_up(500, 8), ht_allocation_size(h->max_allocs, 4));

	TEST_ASSERT_EQUAL(sizeof(*h) + align_up(500, 8), arena_get_usage(arena));
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_hashtable_create);
	RUN_TEST(test_hashtable_use_basic);
	RUN_TEST(test_hashtable_allocation_size);

	return UNITY_END();
}
