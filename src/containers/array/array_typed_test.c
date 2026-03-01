#include <allocators/arena.h>
#include <containers/array_types.h>

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
	{
		i32_array arr = i32_array_create(arena, count);
		TEST_ASSERT_NOT_NULL(arr.raw);
		TEST_ASSERT_EQUAL(count, i32_array_size(arr));
	}
	{
		i16_array arr = i16_array_create(arena, count);
		TEST_ASSERT_NOT_NULL(arr.raw);
		TEST_ASSERT_EQUAL(count, i16_array_size(arr));
	}
	{
		u64_array arr = u64_array_create(arena, count);
		TEST_ASSERT_NOT_NULL(arr.raw);
		TEST_ASSERT_EQUAL(count, u64_array_size(arr));
	}
}

void test_array_typed_set_get(void) {
	size_t const count = 4;
	u16_array arr = u16_array_create(arena, count);
	TEST_ASSERT_NOT_NULL(arr.raw);

	for (size_t i = 0; i < count; ++i) {
		uint16_t t = (uint16_t)i * 2;
		TEST_ASSERT_TRUE(u16_array_set(arr, i, &t));
	}

	for (size_t i = 0; i < count; ++i) {
		uint16_t out = UINT16_MAX;
		TEST_ASSERT_TRUE(u16_array_get(arr, &out, i));
		TEST_ASSERT_EQUAL((uint16_t)i * 2, out);
	}
}

typedef struct temp {
	int a;
	int b;
	char c;
} temp;

ARRAY_DEFINE_TYPE(temp, temp_array)

void test_array_typed_set_get_custom_type(void) {
	size_t const count = 4;
	temp_array arr = temp_array_create(arena, (count));
	TEST_ASSERT_NOT_NULL(arr.raw);

	for (size_t i = 0; i < count; ++i) {
		temp t = {
			.a = (int)(i * 10),
			.b = (int)(-i * 21),
			.c = i,
		};
		TEST_ASSERT_TRUE(temp_array_set(arr, i, &t));
	}

	for (size_t i = 0; i < count; ++i) {
		struct temp out = {0};
		TEST_ASSERT_TRUE(temp_array_get(arr, &out, i));
		TEST_ASSERT_EQUAL_INT((int)(i * 10), out.a);
		TEST_ASSERT_EQUAL_INT((int)(-i * 21), out.b);
		TEST_ASSERT_EQUAL_INT((char)i, out.c);
	}
}

void test_array_typed_at(void) {
	u8_array arr = u8_array_create(arena, 12);
	TEST_ASSERT_NOT_NULL(arr.raw);

	// Use rw to set values:
	for (uint8_t i = 0; i < 12; ++i) {
		uint8_t* elem;
		TEST_ASSERT_TRUE(u8_array_at(arr, &elem, i));
		*elem = i * 3;
	}

	// Use r only to read
	for (size_t i = 0; i < 12; ++i) {
		uint8_t const* elem;
		TEST_ASSERT_TRUE(u8_array_at_const(arr, &elem, i));
		TEST_ASSERT_EQUAL(i * 3, *elem);

		// Compare with array_get:
		uint8_t out;
		TEST_ASSERT_TRUE(u8_array_get(arr, &out, i));
		TEST_ASSERT_EQUAL(i * 3, out);
	}
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_array_typed_create);
	RUN_TEST(test_array_typed_create);
	RUN_TEST(test_array_typed_set_get);
	RUN_TEST(test_array_typed_at);

	return UNITY_END();
}
