#include <strings/path.h>

#include <allocators/arena.h>
#include <containers/array_types.h>
#include <platform/platform.h>
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

	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));
		TEST_ASSERT_EQUAL(s.len, pth.len);
		TEST_ASSERT_EQUAL(0, memcmp(s.str, pth.str, s.len));
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

void test_path_create_invalid(void) {
#define TEST_COUNT 1

	char const* cstrs[TEST_COUNT] = {
		" ",
	};

	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_FALSE(path_create(arena, scratch.arena, s, &pth));
		TEST_ASSERT_EQUAL(0, pth.len);
		TEST_ASSERT_NULL(pth.str);
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

// TODO: multiple trailing slash
void test_path_create_should_change(void) {
#define TEST_COUNT 12
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
		"Documents/.",								// trailing slash+dot
		"Documents/./././.",						// trailing slash+dot chain
		"Documents/././././",						// trailing slash+dot chain
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
		"Documents", // trailing slash+dot
		"Documents", // trailing slash+dot chain
		"Documents", // trailing slash+dot chain
	};

	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		char const* want = expect[i];
		size_t want_len = strlen(want);
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));
		TEST_ASSERT_EQUAL(want_len, pth.len);
		TEST_ASSERT_EQUAL(0, memcmp(want, pth.str, want_len));
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

void test_path_get_filename(void) {
#define TEST_COUNT 8
	char const* cstrs[TEST_COUNT] = {
		"/a",
		"build/src/././strings/path/path_test.c",
		"./././././././././././.",
		"Documents/",
		"Documents/somefile.txt/",
		"/home/User/Documents/",
		"empty/filename/    ",
		"a",
	};

	char const* expect[TEST_COUNT] = {
		"a",			//
		"path_test.c",	//
		".",			//
		"Documents",	//
		"somefile.txt", //
		"Documents",	//
		"    ",			//
		"a",			//
	};

	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		char const* want = expect[i];
		size_t want_len = strlen(want);
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));

		path res;
		TEST_ASSERT_TRUE_MESSAGE(path_get_filename(pth, &res), want);
		TEST_ASSERT_EQUAL_MESSAGE(want_len, res.len, want);
		TEST_ASSERT_EQUAL(0, strncmp(want, res.str, want_len));
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

void test_path_get_filename_fail(void) {
#define TEST_COUNT 3
	// clang-format off
	char const* cstrs[TEST_COUNT] = {
		"/",
		"/.",
		"/./././././././././././.",
	};
	// clang-format on
	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));

		path ret;
		TEST_ASSERT_FALSE_MESSAGE(path_get_filename(pth, &ret), cstrs[i]);
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

void test_path_get_extension(void) {
#define TEST_COUNT 5
	char const* cstrs[TEST_COUNT] = {
		"build/src/././strings/path/path_test.c", // basic
		"Documents/somefile.txt/",				  // trailing /
		"/home/User/.hidden.tar.gz",			  // many dots
		"/home/User/weird_ext.  ",				  // whitespace extension
		"/home/User/  .hidden",					  // leading space in filename
	};

	char const* expect[TEST_COUNT] = {
		"c",	  //
		"txt",	  //
		"gz",	  //
		"  ",	  //
		"hidden", //
	};

	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		char const* want = expect[i];
		size_t want_len = strlen(want);
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));

		path res;
		TEST_ASSERT_TRUE_MESSAGE(path_get_extension(pth, &res), want);
		TEST_ASSERT_EQUAL_MESSAGE(want_len, res.len, want);
		TEST_ASSERT_EQUAL(0, strncmp(want, res.str, want_len));
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

void test_path_get_extension_fail(void) {
#define TEST_COUNT 7
	// clang-format off
	char const* cstrs[TEST_COUNT] = {
		"/",
		"/.",
		"/./././././././././././.",
		"/home/User/.",
		"/home/User/Documents.",
		"/home/User/.hidden",
		"/home/User/no_ext"
	};
	// clang-format on
	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));

		path ret;
		TEST_ASSERT_FALSE_MESSAGE(path_get_extension(pth, &ret), cstrs[i]);
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

void test_path_get_stem(void) {
#define TEST_COUNT 9
	char const* cstrs[TEST_COUNT] = {
		"build/src/././strings/path/path_test.c", // basic
		"Documents/somefile.txt/",				  // trailing /
		"/home/User/.hidden.tar.gz",			  // many dots
		"/archive.tar.gz",						  // absolute many dots
		"/home/User/.profile",					  // hidden easy
		"/home/User/weird_ext.  ",				  // whitespace extension
		"/home/User/  .hidden",					  // leading space in filename
		"README",								  //
		"/home/User/.",							  // trailing '/.' gets trimmed
	};

	// clang-format off
	char const* expect[TEST_COUNT] = {
		"path_test",
		"somefile",
		".hidden.tar",
		"archive.tar",
		".profile",
		"weird_ext",
		"  ",
		"README",
		"User",
	};
	// clang-format on

	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		char const* want = expect[i];
		size_t want_len = strlen(want);
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));

		path res;
		TEST_ASSERT_TRUE_MESSAGE(path_get_stem(pth, &res), want);
		TEST_ASSERT_EQUAL_MESSAGE(want_len, res.len, want);
		TEST_ASSERT_EQUAL(0, strncmp(want, res.str, want_len));
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

/**
 * @note path_get_stem only fails if path_get_filename fails.
 */
void test_path_get_stem_fail(void) {
#define TEST_COUNT 3
	// clang-format off
	char const* cstrs[TEST_COUNT] = {
		"/",
		"/.",
		"/./././././././././././.",
	};
	// clang-format on
	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, cstrs[i]));

		path pth;
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, s, &pth));

		path ret;
		TEST_ASSERT_FALSE_MESSAGE(path_get_stem(pth, &ret), cstrs[i]);
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

void test_path_join(void) {
#define TEST_COUNT 5
	// clang-format off
	char const* left[TEST_COUNT] = {
		"build/src",
		"/home/User/./.",
		"/home/User/./.",
		"/./././././././././././.",
		"/////"
	};
	char const* right[TEST_COUNT] = {
		"strings/path/path_test.c",
		"Documents/somefile.txt",
		"./././Documents/somefile.txt",
		"home",
		"home",
	};
	char const* expect[TEST_COUNT] = {
		"build/src/strings/path/path_test.c",
		"/home/User/Documents/somefile.txt",
		"/home/User/Documents/somefile.txt",
		"/home",
		"/home",
	};
	// clang-format on
	scratch_arena scratch;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	for (size_t i = 0; i < TEST_COUNT; ++i) {
		string l, r;
		TEST_ASSERT_TRUE(string_create(arena, &l, left[i]));
		TEST_ASSERT_TRUE(string_create(arena, &r, right[i]));

		path l_pth, r_pth;
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, l, &l_pth));
		TEST_ASSERT_TRUE(path_create(arena, scratch.arena, r, &r_pth));

		char const* want = expect[i];
		size_t want_len = strlen(want);

		path res;
		TEST_ASSERT_TRUE_MESSAGE(
			path_join(arena, scratch.arena, l_pth, r_pth, &res), want);
		TEST_ASSERT_EQUAL_MESSAGE(want_len, res.len, want);
		TEST_ASSERT_EQUAL(0, strncmp(want, res.str, want_len));
	}
	arena_clear_scratch(scratch);
#undef TEST_COUNT
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_path_create_valid);
	RUN_TEST(test_path_create_invalid);
	RUN_TEST(test_path_create_should_change);
	RUN_TEST(test_path_get_filename);
	RUN_TEST(test_path_get_filename_fail);
	RUN_TEST(test_path_get_extension);
	RUN_TEST(test_path_get_extension_fail);
	RUN_TEST(test_path_get_stem);
	RUN_TEST(test_path_get_stem_fail);
	RUN_TEST(test_path_join);

	return UNITY_END();
}
