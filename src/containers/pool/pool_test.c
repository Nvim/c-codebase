#include <containers/pool.h>

#include <allocators/arena.h>
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

void tearDown(void) { arena_destroy(arena); }

typedef struct testing {
	int a;
	float b;
	char c[8];
} testing;

POOL_DEFINE_TYPE(testing, testing_pool)

void pool_test_create(void) {
	pool_t* pool = pool_create(arena, 16, sizeof(int));
	TEST_ASSERT_NOT_NULL(pool);
}

void pool_test_new_and_get(void) {
	pool_t* pool = pool_create(arena, 16, sizeof(int));

	int value = 42;
	pool_handle h = pool_new(pool, &value);

	TEST_ASSERT_NOT_EQUAL(POOL_INVALID_HANDLE, h.handle);

	int* retrieved = (int*)pool_get(pool, h);
	TEST_ASSERT_NOT_NULL(retrieved);
	TEST_ASSERT_EQUAL_INT(42, *retrieved);
}

void pool_test_memcpy_semantics(void) {
	pool_t* pool = pool_create(arena, 16, sizeof(int));

	int value = 10;
	pool_handle h = pool_new(pool, &value);

	value = 99; // modify original

	int* retrieved = (int*)pool_get(pool, h);
	TEST_ASSERT_EQUAL_INT(10, *retrieved); // should not change
}

void pool_test_get(void) {
	testing_pool p = testing_pool_create(arena, 12);

	TEST_ASSERT_NOT_NULL(p.raw);

	testing expect = {
		.a = 42,
		.b = 0.234f,
		.c = "abcdef",
	};

	testing_pool_handle handle = testing_pool_new(p, &expect);
	TEST_ASSERT_TRUE(testing_pool_is_handle_valid(p, handle));

	testing* got = testing_pool_get(p, handle);
	TEST_ASSERT_EQUAL(expect.a, got->a);
	TEST_ASSERT_EQUAL(expect.b, got->b);
	TEST_ASSERT_EQUAL(0, strcmp(expect.c, got->c));

	expect.a = 43242;

	testing_pool_handle handle2 = testing_pool_new(p, &expect);
	TEST_ASSERT_TRUE(testing_pool_is_handle_valid(p, handle2));

	testing* got2 = testing_pool_get(p, handle2);
	TEST_ASSERT_EQUAL(expect.a, got2->a);
	TEST_ASSERT_EQUAL(expect.b, got2->b);
	TEST_ASSERT_EQUAL(0, strcmp(expect.c, got2->c));
}

void pool_test_handle_validity(void) {
	pool_t* pool = pool_create(arena, 16, sizeof(int));

	int v = 1;
	pool_handle h = pool_new(pool, &v);

	TEST_ASSERT_TRUE(pool_is_handle_valid(pool, h));

	pool_release(pool, h);

	TEST_ASSERT_FALSE(pool_is_handle_valid(pool, h));
}

void pool_test_release_invalidates_access(void) {
	pool_t* pool = pool_create(arena, 16, sizeof(int));

	int v = 5;
	pool_handle h = pool_new(pool, &v);

	pool_release(pool, h);

	TEST_ASSERT_NULL(pool_get(pool, h));
}

void pool_test_reset_invalidates_all(void) {
	pool_t* pool = pool_create(arena, 16, sizeof(int));

	int v1 = 1, v2 = 2;

	pool_handle h1 = pool_new(pool, &v1);
	pool_handle h2 = pool_new(pool, &v2);

	pool_reset(pool);

	TEST_ASSERT_FALSE(pool_is_handle_valid(pool, h1));
	TEST_ASSERT_FALSE(pool_is_handle_valid(pool, h2));
}

void pool_test_multiple_allocations(void) {
	pool_t* pool = pool_create(arena, 4, sizeof(int));

	int values[4] = {1, 2, 3, 4};
	pool_handle handles[4];

	for (int i = 0; i < 4; ++i) handles[i] = pool_new(pool, &values[i]);

	for (int i = 0; i < 4; ++i) {
		int* v = (int*)pool_get(pool, handles[i]);
		TEST_ASSERT_EQUAL_INT(values[i], *v);
	}
}

void pool_test_reuse_after_release(void) {
	pool_t* pool = pool_create(arena, 2, sizeof(int));

	int a = 1, b = 2, c = 3;

	pool_handle h1 = pool_new(pool, &a);
	UNUSED pool_handle h2 = pool_new(pool, &b);

	pool_release(pool, h1);

	pool_handle h3 = pool_new(pool, &c);

	TEST_ASSERT_TRUE(pool_is_handle_valid(pool, h3));

	int* v = (int*)pool_get(pool, h3);
	TEST_ASSERT_EQUAL_INT(3, *v);
}

// -----------------------------
// Typed wrapper tests
// -----------------------------

typedef struct {
	int x;
	float y;
} dummy_t;

POOL_DEFINE_TYPE(dummy_t, dummy_pool)

void pool_test_typed_create_and_basic_usage(void) {
	dummy_pool pool = dummy_pool_create(arena, 8);

	dummy_t d = {.x = 7, .y = 3.14f};

	dummy_pool_handle h = dummy_pool_new(pool, &d);

	TEST_ASSERT_TRUE(dummy_pool_is_handle_valid(pool, h));

	dummy_t* retrieved = dummy_pool_get(pool, h);

	TEST_ASSERT_EQUAL_INT(7, retrieved->x);
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.14f, retrieved->y);
}

void pool_test_typed_release(void) {
	dummy_pool pool = dummy_pool_create(arena, 8);

	dummy_t d = {1, 2.0f};
	dummy_pool_handle h = dummy_pool_new(pool, &d);

	dummy_pool_release(pool, h);

	TEST_ASSERT_FALSE(dummy_pool_is_handle_valid(pool, h));
}

void pool_test_typed_reset(void) {
	dummy_pool pool = dummy_pool_create(arena, 8);

	dummy_t d = {5, 6.0f};

	dummy_pool_handle h = dummy_pool_new(pool, &d);

	dummy_pool_reset(pool);

	TEST_ASSERT_FALSE(dummy_pool_is_handle_valid(pool, h));
}

void pool_test_typed_handle_wrap_unwrap(void) {
	pool_handle raw = {.handle = 123};

	dummy_pool_handle wrapped = dummy_pool_wrap(raw);
	pool_handle unwrapped = dummy_pool_unwrap(wrapped);

	TEST_ASSERT_EQUAL_UINT32(raw.handle, unwrapped.handle);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(pool_test_create);
	RUN_TEST(pool_test_new_and_get);
	RUN_TEST(pool_test_get);
	RUN_TEST(pool_test_memcpy_semantics);
	RUN_TEST(pool_test_handle_validity);
	RUN_TEST(pool_test_release_invalidates_access);
	RUN_TEST(pool_test_reset_invalidates_all);
	RUN_TEST(pool_test_multiple_allocations);
	RUN_TEST(pool_test_reuse_after_release);

	RUN_TEST(pool_test_typed_create_and_basic_usage);
	RUN_TEST(pool_test_typed_release);
	RUN_TEST(pool_test_typed_reset);
	RUN_TEST(pool_test_typed_handle_wrap_unwrap);

	return UNITY_END();
}
