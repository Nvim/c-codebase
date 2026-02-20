#include <containers/arena.h>
#include <containers/array.h>

#include <unity.h>

static arena_t* arena = NULL;

void setUp(void) {
	arena = arena_create(1024);
	TEST_ASSERT_NOT_NULL(arena);
}

void tearDown(void) {
	if (arena) {
		arena_destroy(arena);
		arena = NULL;
	}
}

#define CREATE_TYPED_ARRAY(TYPE, SIZE)                                         \
	((TYPE*)array_create(arena, (SIZE), sizeof(TYPE)))

void test_array_create_success(void) {
	size_t const count = 10;
	array_t* arr = array_create(arena, count, sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);
	TEST_ASSERT_EQUAL_UINT32(count, array_size(arr));
	TEST_ASSERT_EQUAL_UINT32(sizeof(int), array_stride(arr));
}

void test_array_create_null_arena(void) {
	array_t* arr = array_create(NULL, 5, sizeof(int));
	TEST_ASSERT_NULL(arr);
}

void test_array_size_invalid_pointer(void) {
	TEST_ASSERT_EQUAL_UINT32(0, array_size(NULL));
}

void test_array_stride_invalid_pointer(void) {
	TEST_ASSERT_EQUAL_UINT32(0, array_stride(NULL));
}

void test_array_set_get(void) {
	size_t const count = 4;
	struct temp {
		int a;
		int b;
		char c;
	};
	array_t* arr = array_create(arena, (count), sizeof(struct temp));
	TEST_ASSERT_NOT_NULL(arr);

	for (size_t i = 0; i < count; ++i) {
		struct temp t = {
			.a = (int)(i * 10),
			.b = (int)(-i * 21),
			.c = i,
		};
		TEST_ASSERT_TRUE(array_set(arr, i, &t));
	}

	for (size_t i = 0; i < count; ++i) {
		struct temp out = { 0 };
		TEST_ASSERT_TRUE(array_get(arr, &out, i));
		TEST_ASSERT_EQUAL_INT((int)(i * 10), out.a);
		TEST_ASSERT_EQUAL_INT((int)(-i * 21), out.b);
		TEST_ASSERT_EQUAL_INT((char)i, out.c);
	}
}

void test_array_set_out_of_bounds(void) {
	size_t const count = 3;
	array_t* arr = array_create(arena, (count), sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);

	int dummy = 42;
	TEST_ASSERT_FALSE(array_set(arr, count, &dummy));
}

void test_array_get_out_of_bounds(void) {
	array_t* arr = array_create(arena, 2, sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);

	int out = 0;
	TEST_ASSERT_FALSE(array_get(arr, &out, 2));
	TEST_ASSERT_EQUAL(0, out);
}

void test_array_clear_zeros_memory(void) {
	size_t const count = 5;
	array_t* arr = array_create(arena, (count), sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);

	for (size_t i = 0; i < count; ++i) {
		int v = (int)i + 1;
		TEST_ASSERT_TRUE(array_set(arr, i, &v));
	}

	array_clear(arr);

	for (size_t i = 0; i < count; ++i) {
		int out = -1;
		TEST_ASSERT_TRUE(array_get(arr, &out, i));
		TEST_ASSERT_EQUAL_INT(0, out);
	}
}

void test_macro_compiles(void) {
	array_t* a = array_create_typed(arena, 28, int);
	TEST_ASSERT_NOT_NULL(a);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_array_create_success);
	RUN_TEST(test_array_create_null_arena);
	RUN_TEST(test_array_size_invalid_pointer);
	RUN_TEST(test_array_stride_invalid_pointer);
	RUN_TEST(test_array_set_get);
	RUN_TEST(test_array_set_out_of_bounds);
	RUN_TEST(test_array_get_out_of_bounds);
	RUN_TEST(test_array_clear_zeros_memory);
	RUN_TEST(test_macro_compiles);

	return UNITY_END();
}
