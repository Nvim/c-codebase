#include "hashtable_internal.h"

#include <allocators/arena.h>
#include <containers/hashtable.h>
#include <containers/hashtable_types.h>
#include <platform/platform.h>
#include <strings/string.h>

#include <string.h>
#include <unity.h>

static arena_t* arena = NULL;

void setUp(void) {
	arena = arena_create(1024);
	TEST_ASSERT_TRUE(arena_pool_init());
	TEST_ASSERT_NOT_NULL(arena);
}

void tearDown(void) {}

void test_hashtable_create(void) {
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

	hashtable_t* ht = hashtable_create(arena, 100, sizeof(int), NULL);

	{ // Init
		TEST_ASSERT_NOT_NULL(arena);
		TEST_ASSERT_NOT_NULL(ht);

		TEST_ASSERT_TRUE(string_create(arena, &key, "key_one"));
		TEST_ASSERT_TRUE(string_create(arena, &key2, "key_two"));
	}

	{ // Insert & Get
		TEST_ASSERT_TRUE(hashtable_insert(ht, key, &elem));
		TEST_ASSERT_TRUE(hashtable_insert(ht, key2, &elem2));

		TEST_ASSERT_EQUAL(2, hashtable_usage(ht));

		TEST_ASSERT_TRUE(hashtable_get(ht, &res1, key));
		TEST_ASSERT_TRUE(hashtable_get(ht, &res2, key2));

		TEST_ASSERT_EQUAL(elem, res1);
		TEST_ASSERT_EQUAL(elem2, res2);
	}

	{ // Get invalid key
		key.str[6] = 'z';
		TEST_ASSERT_FALSE(hashtable_get(ht, &res1, key));
	}

	{ // Reset
		hashtable_reset(ht);
		TEST_ASSERT_FALSE(hashtable_get(ht, &res2, key2));
		TEST_ASSERT_EQUAL(0, hashtable_usage(ht));
	}
}

void test_hashtable_allocation_size(void) {
	TEST_ASSERT_EQUAL(.8f, HT_RATIO);

	hashtable_t* ht = hashtable_create(arena, 100, sizeof(int), hash_default);
	TEST_ASSERT_NOT_NULL(ht);

	ht_impl* h = (ht_impl*)ht;
	TEST_ASSERT_EQUAL(100, h->max_allocs);
	// 400*1.2 = 500
	TEST_ASSERT_EQUAL(align_up(500, 8), ht_allocation_size(h->max_allocs, 4));

	TEST_ASSERT_EQUAL(sizeof(*h) + align_up(500, 8), arena_get_usage(arena));
}

void test_hashtable_types(void) {
	i16_hashtable ht = i16_hashtable_create(arena, 12, hash_default);
	TEST_ASSERT_NOT_NULL(ht.raw);
	TEST_ASSERT_EQUAL(0, i16_hashtable_usage(ht));

	TEST_ASSERT_TRUE(i16_hashtable_insert(ht, str("first"), 44));
	TEST_ASSERT_TRUE(i16_hashtable_insert(ht, str("second"), 4096));

	TEST_ASSERT_EQUAL(2, i16_hashtable_usage(ht));

	int16_t res1, res2;
	TEST_ASSERT_TRUE(i16_hashtable_get(ht, &res1, str("first")));
	TEST_ASSERT_TRUE(i16_hashtable_get(ht, &res2, str("second")));

	TEST_ASSERT_EQUAL(44, res1);
	TEST_ASSERT_EQUAL(4096, res2);
}

typedef struct sometype {
	int a;
	float b;
	size_t c[3];
} sometype;

HASHTABLE_DEFINE_PTR_TYPE(sometype, ptr_tbl)

void test_hashtable_ptr_types(void) {
	ptr_tbl ht = ptr_tbl_create(arena, 8, hash_default);
	TEST_ASSERT_NOT_NULL(ht.raw);
	TEST_ASSERT_EQUAL(0, ptr_tbl_usage(ht));

	sometype a = {.a = 8, .b = 11.21f, .c = {20, 40, 60}};
	sometype b = {.a = -8, .b = -11.21f, .c = {80, 160, 240}};

	TEST_ASSERT_TRUE(ptr_tbl_insert(ht, str("first"), &a));
	TEST_ASSERT_TRUE(ptr_tbl_insert(ht, str("second"), &b));

	TEST_ASSERT_EQUAL(2, ptr_tbl_usage(ht));

	sometype* res1 = ptr_tbl_get(ht, str("first"));
	sometype* res2 = ptr_tbl_get(ht, str("second"));

	TEST_ASSERT_EQUAL(&a, res1);
	TEST_ASSERT_EQUAL(&b, res2);

	{
		TEST_ASSERT_EQUAL(b.a, res2->a);
		TEST_ASSERT_EQUAL(b.b, res2->b);
		TEST_ASSERT_EQUAL(0, memcmp(b.c, res2->c, 3 * sizeof(size_t)));

		TEST_ASSERT_EQUAL(a.a, res1->a);
		TEST_ASSERT_EQUAL(a.b, res1->b);
		TEST_ASSERT_EQUAL(0, memcmp(a.c, res1->c, 3 * sizeof(size_t)));
	}
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_hashtable_create);
	RUN_TEST(test_hashtable_use_basic);
	RUN_TEST(test_hashtable_allocation_size);
	RUN_TEST(test_hashtable_types);
	RUN_TEST(test_hashtable_ptr_types);

	return UNITY_END();
}
