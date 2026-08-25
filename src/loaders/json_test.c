#include "json_internal.h"
#include <filesystem/filesystem.h>
#include <loaders/json_loader.h>
#include <platform/platform.h>

#include <common.h>

#include <string.h>
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

void test_json_read_whitespace_good(void) {
	char const* txt = "   \t \n\n \t hello world";
	TEST_ASSERT_TRUE(json_read_whitespace(&txt));
	TEST_ASSERT_EQUAL_CHAR('h', *txt);
}

void test_json_read_bool_good(void) {
	char const* true_str = "true";
	bool out = false;
	TEST_ASSERT_TRUE(json_read_bool(&true_str, &out));
	TEST_ASSERT_TRUE(out);

	char const* false_str = "false";
	TEST_ASSERT_TRUE(json_read_bool(&false_str, &out));
	TEST_ASSERT_FALSE(out);
}

void test_json_read_number_good(void) {
	char const* num_str = "144";
	double out = 0.f;

	TEST_ASSERT_TRUE(json_read_number(&num_str, &out));
	TEST_ASSERT_EQUAL(144, out); // TODO: enable unity double precision
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_json_read_whitespace_good);
	RUN_TEST(test_json_read_bool_good);
	RUN_TEST(test_json_read_number_good);

	return UNITY_END();
}
