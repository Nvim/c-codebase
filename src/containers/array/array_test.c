#include <allocators/arena.h>
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

void test_array_typed_create(void) {
	size_t const count = 10;
	array_t* arr = _array_create(arena, count, sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);
	TEST_ASSERT_EQUAL_UINT32(count, _array_size(arr));
	TEST_ASSERT_EQUAL_UINT32(sizeof(int), _array_stride(arr));
}

void test_array_create_null_arena(void) {
	array_t* arr = _array_create(NULL, 5, sizeof(int));
	TEST_ASSERT_NULL(arr);
}

void test_array_size_invalid_pointer(void) {
	TEST_ASSERT_EQUAL_UINT32(0, _array_size(NULL));
}

void test_array_stride_invalid_pointer(void) {
	TEST_ASSERT_EQUAL_UINT32(0, _array_stride(NULL));
}

void test_array_typed_set_get(void) {
	size_t const count = 4;
	struct temp {
		int a;
		int b;
		char c;
	};
	array_t* arr = _array_create(arena, (count), sizeof(struct temp));
	TEST_ASSERT_NOT_NULL(arr);

	for (size_t i = 0; i < count; ++i) {
		struct temp t = {
			.a = (int)(i * 10),
			.b = (int)(-i * 21),
			.c = i,
		};
		TEST_ASSERT_TRUE(_array_set(arr, &t, i));
	}

	for (size_t i = 0; i < count; ++i) {
		struct temp out = {0};
		TEST_ASSERT_TRUE(_array_get(arr, &out, i));
		TEST_ASSERT_EQUAL_INT((int)(i * 10), out.a);
		TEST_ASSERT_EQUAL_INT((int)(-i * 21), out.b);
		TEST_ASSERT_EQUAL_INT((char)i, out.c);
	}
}

void test_array_set_out_of_bounds(void) {
	size_t const count = 3;
	array_t* arr = _array_create(arena, (count), sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);

	int dummy = 42;
	TEST_ASSERT_FALSE(_array_set(arr, &dummy, count));
}

void test_array_get_out_of_bounds(void) {
	array_t* arr = _array_create(arena, 2, sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);

	int out = 0;
	TEST_ASSERT_FALSE(_array_get(arr, &out, 2));
	TEST_ASSERT_EQUAL(0, out);
}

void test_array_clear_zeros_memory(void) {
	size_t const count = 5;
	array_t* arr = _array_create(arena, (count), sizeof(int));
	TEST_ASSERT_NOT_NULL(arr);

	for (size_t i = 0; i < count; ++i) {
		int v = (int)i + 1;
		TEST_ASSERT_TRUE(_array_set(arr, &v, i));
	}

	_array_clear(arr);

	for (size_t i = 0; i < count; ++i) {
		int out = -1;
		TEST_ASSERT_TRUE(_array_get(arr, &out, i));
		TEST_ASSERT_EQUAL_INT(0, out);
	}
}

// TODO: fix
void test_array_at_rw(void) {
	array_t* arr = _array_create(arena, 12, sizeof(size_t));
	TEST_ASSERT_NOT_NULL(arr);

	for (size_t i = 0; i < 12; ++i) {
		int* elem;
		TEST_ASSERT_TRUE(_array_at(arr, (void**)&elem, i));
		*elem = i * 2;
	}
	for (size_t i = 0; i < 12; ++i) {
		int out;
		TEST_ASSERT_TRUE(_array_get(arr, &out, i));
		TEST_ASSERT_EQUAL(i * 2, out);

		int* elem;
		TEST_ASSERT_TRUE(_array_at(arr, (void**)&elem, i));
		TEST_ASSERT_EQUAL(i * 2, *elem);
	}
}

void test_array_at_readonly(void) {
	array_t* arr = _array_create(arena, 12, sizeof(size_t));
	TEST_ASSERT_NOT_NULL(arr);

	// Use rw to set values:
	for (size_t i = 0; i < 12; ++i) {
		int* elem;
		TEST_ASSERT_TRUE(_array_at(arr, (void**)&elem, i));
		*elem = i * 3;
	}

	// Use r only to read
	for (size_t i = 0; i < 12; ++i) {
		int const* elem;
		TEST_ASSERT_TRUE(_array_at_const(arr, (void const**)&elem, i));
		TEST_ASSERT_EQUAL(i * 3, *elem);

		// Compare with array_get:
		int out;
		TEST_ASSERT_TRUE(_array_get(arr, &out, i));
		TEST_ASSERT_EQUAL(i * 3, out);
	}
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_array_typed_create);
	RUN_TEST(test_array_create_null_arena);
	RUN_TEST(test_array_size_invalid_pointer);
	RUN_TEST(test_array_stride_invalid_pointer);
	RUN_TEST(test_array_typed_set_get);
	RUN_TEST(test_array_set_out_of_bounds);
	RUN_TEST(test_array_get_out_of_bounds);
	RUN_TEST(test_array_clear_zeros_memory);
	// RUN_TEST(test_array_at_rw);
	RUN_TEST(test_array_at_readonly);

	return UNITY_END();
}
