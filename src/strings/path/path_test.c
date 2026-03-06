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
#define TEST_COUNT 6

	// All these shouldn't be modified
	char const* cstrs[TEST_COUNT] = {
		"build/src/strings/path/path_test.c", // regular
		"/home/User/Documents",				  // absolute
		"/home/I contain spaces/file.txt",	  // spaces
		"/",								  // root
		"/  ",								  // root + spaces
		"."									  // dot
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

// TODO: multiple trailing slash
void test_path_create_should_change(void) {
#define TEST_COUNT 9
	// All these are valid, but should be modified
	char const* cstrs[TEST_COUNT] = {
		"build/src/././strings/path/path_test.c",	// dots
		"./././././././././././.",					// lots of dots
		"/.",										// absolute dot
		"/./././././././././././.",					// absolute dots
		"Documents/",								// trailing slash
		"Documents/somefile.txt/",					// trailing slash
		"/home/User/Documents/",					// absolute + trail
		"/home////too much//file.txt",				// many slashes
		"build//./src///strings/path/path_test.c/", // a lot
	};
	char const* expect[TEST_COUNT] = {
		"build/src/strings/path/path_test.c",
		".",
		"/",
		"/",
		"Documents",
		"Documents/somefile.txt",
		"/home/User/Documents",
		"/home/too much/file.txt",
		"build/src/strings/path/path_test.c",
	};

	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		char const* want = expect[i];
		size_t want_len = strlen(want);
		TEST_ASSERT_TRUE(path_create(arena, s, &pth));
		TEST_ASSERT_EQUAL(want_len, pth.len);
		TEST_ASSERT_EQUAL(0, memcmp(want, pth.str, want_len));
	}
#undef TEST_COUNT
}

// TODO: /a
void test_path_get_filename(void) {
#define TEST_COUNT 7
	char const* cstrs[TEST_COUNT] = {
		"build/src/././strings/path/path_test.c",
		"./././././././././././.",
		"Documents/",
		"Documents/somefile.txt/",
		"/home/User/Documents/",
		"empty/filename/    ",
		"a",
		// "/a",
	};

	char const* expect[TEST_COUNT] = {
		"path_test.c",	//
		".",			//
		"Documents",	//
		"somefile.txt", //
		"Documents",	//
		"    ",			//
		"a",			//
		// "a",			//
	};

	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		char const* want = expect[i];
		size_t want_len = strlen(want);
		TEST_ASSERT_TRUE(path_create(arena, s, &pth));

		path res;
		TEST_ASSERT_TRUE_MESSAGE(path_get_filename(pth, &res), want);
		TEST_ASSERT_EQUAL_MESSAGE(want_len, res.len, want);
		TEST_ASSERT_EQUAL(0, strncmp(want, res.str, want_len));
	}
#undef TEST_COUNT
}

void test_path_get_filename_fail(void) {
#define TEST_COUNT 3
	char const* cstrs[TEST_COUNT] = {
		"/",
		"/.",
		"/./././././././././././.",
	};
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_TRUE(path_create(arena, s, &pth));

		path ret;
		TEST_ASSERT_FALSE_MESSAGE(path_get_filename(pth, &ret), cstrs[i]);
	}
#undef TEST_COUNT
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_path_create_valid);
	RUN_TEST(test_path_create_invalid);
	RUN_TEST(test_path_create_should_change);
	RUN_TEST(test_path_get_filename);
	RUN_TEST(test_path_get_filename_fail);

	return UNITY_END();
}
