#include <containers/string.h>
#include <containers/arena.h>
#include <containers/hashtable.h>
#include <core/platform.h>
#include <unity.h>

void setUp(void) {}

void tearDown(void) {}

void test_hashtable_create(void) {
	arena_t* arena = arena_create(4032);
	TEST_ASSERT_NOT_NULL(arena);

	hashtable_t* ht = hashtable_create(arena, 100, sizeof(int), hash_default);
	TEST_ASSERT_NOT_NULL(ht);
}

void test_hashtable_use_basic(void) {
	arena_t* arena = arena_create(4032);
	TEST_ASSERT_NOT_NULL(arena);

	hashtable_t* ht = hashtable_create(arena, 100, sizeof(int), hash_default);
	TEST_ASSERT_NOT_NULL(ht);

	string key;
	string key2;
	int elem = 28;
	int elem2 = 42;

	TEST_ASSERT_TRUE(string_create(arena, &key, "key_one"));
	TEST_ASSERT_TRUE(string_create(arena, &key2, "key_two"));

	TEST_ASSERT_TRUE(hashtable_insert(ht, &key, &elem));
	TEST_ASSERT_TRUE(hashtable_insert(ht, &key2, &elem2));

	int res1, res2;
	TEST_ASSERT_TRUE(hashtable_get(ht, &res1, &key));
	TEST_ASSERT_TRUE(hashtable_get(ht, &res2, &key2));
	TEST_ASSERT_EQUAL(elem, res1);
	TEST_ASSERT_EQUAL(elem2, res2);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_hashtable_create);
	RUN_TEST(test_hashtable_use_basic);

	return UNITY_END();
}
