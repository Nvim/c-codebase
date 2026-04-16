#include <filesystem/filesystem.h>

#include <common.h>

#include <string.h>
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

void test_file_read_all(void) {
	scratch_arena scratch;
	path p;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	TEST_ASSERT_TRUE(
		path_create(arena, scratch.arena, str("src/filesystem/test.txt"), &p));

	fs_file_read_result ret = fs_read_all_file(arena, p);
	TEST_ASSERT_TRUE(ret.ok);
	TEST_ASSERT_EQUAL(27, u8_array_size(ret.bytes));

	// Bytes:
	char expect[28] = "line 1\nline 2\nline 3\ncool!\n";
	TEST_ASSERT_EQUAL(0, memcmp(u8_array_begin(ret.bytes), expect, 27));

	// String:
	string expect_str = str("line 1\nline 2\nline 3\ncool!\n");
	TEST_ASSERT_EQUAL(27, expect_str.len);
	TEST_ASSERT_TRUE(string_compare(expect_str, ret.str));
}

void test_file_read_some(void) {
	scratch_arena scratch;
	path p;
	TEST_ASSERT_TRUE(arena_get_scratch(NULL, &scratch));
	TEST_ASSERT_TRUE(
		path_create(arena, scratch.arena, str("src/filesystem/test.txt"), &p));

	fs_file_read_result ret = fs_read_file(arena, p, 7);
	TEST_ASSERT_TRUE(ret.ok);
	TEST_ASSERT_EQUAL(7, u8_array_size(ret.bytes));

	// Bytes:
	char expect[8] = "line 1\n";
	TEST_ASSERT_EQUAL(0, memcmp(u8_array_begin(ret.bytes), expect, 7));

	// String:
	string expect_str = str("line 1\n");
	TEST_ASSERT_EQUAL(7, expect_str.len);
	TEST_ASSERT_TRUE(string_compare(ret.str, expect_str));
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_file_read_all);
	RUN_TEST(test_file_read_some);

	return UNITY_END();
}
