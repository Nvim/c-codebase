#include <strings/path.h>

#include <allocators/arena.h>
#include <containers/array_types.h>
#include <core/platform.h>
#include <string.h>
#include <strings/string.h>
#include <unity.h>

static arena_t* arena = NULL;

void setUp(void) {
	arena = arena_create(1024);
	TEST_ASSERT_TRUE(arena_pool_init());
	TEST_ASSERT_NOT_NULL(arena);
}

void tearDown(void) {
	if (arena) {
		arena_destroy(arena);
		arena = NULL;
	}
}

void test_path_create_valid(void) {
#define TEST_COUNT 5

	// All these shouldn't be modified
	char const* cstrs[TEST_COUNT] = {
		"build/src/strings/path/path_test.c", // regular
		"/home/User/Documents",				  // absolute
		"/home/I contain spaces/file.txt",	  // spaces
		"/",								  // root
		"/  ",								  // root + spaces
	};

	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_TRUE(path_create(arena, s, &pth));
		TEST_ASSERT_EQUAL(s.len, pth.len);
		TEST_ASSERT_EQUAL(0, memcmp(s.str, pth.str, s.len));
	}
#undef TEST_COUNT
}

void test_path_create_invalid(void) {
#define TEST_COUNT 1

	char const* cstrs[TEST_COUNT] = {
		" ",
	};

	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_FALSE(path_create(arena, s, &pth));
		TEST_ASSERT_EQUAL(0, pth.len);
		TEST_ASSERT_NULL(pth.str);
	}
#undef TEST_COUNT
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_path_create_valid);
	RUN_TEST(test_path_create_invalid);

	return UNITY_END();
}
