#include <containers/arena.h>
#include <containers/string.h>
#include <core/platform.h>
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

void test_string_create(void) {
	char const* cstr = "Hello World";

	string s;
	size_t prev_usage = arena_get_usage(arena);
	TEST_ASSERT_TRUE(string_create(arena, &s, cstr));
	TEST_ASSERT_EQUAL(strlen(cstr), s.len);
	TEST_ASSERT_EQUAL(align_up(prev_usage + 11, 8), arena_get_usage(arena));
}

void test_string_create_empty(void) {
	char const* cstr = "";

	string s;
	TEST_ASSERT_FALSE(string_create(arena, &s, cstr));
}

void test_string_copy(void) {
	string s;
	TEST_ASSERT_TRUE(string_create(arena, &s, "four"));

	string s2;
	TEST_ASSERT_TRUE(string_copy(arena, &s2, &s));
	TEST_ASSERT_NOT_EQUAL(s2.str, s.str);
	TEST_ASSERT_EQUAL(s2.len, s.len);
}

void test_string_append(void) {
	string s;
	TEST_ASSERT_TRUE(string_create(arena, &s, "hello"));

	string res;
	TEST_ASSERT_FALSE(string_append(arena, &res, NULL, 'z'));
	TEST_ASSERT_FALSE(string_append(arena, NULL, &s, 'z'));
	TEST_ASSERT_TRUE(string_append(arena, &res, &s, 'z'));
	TEST_ASSERT_EQUAL(6, res.len);
	TEST_ASSERT_EQUAL('h', res.str[0]);
	TEST_ASSERT_EQUAL('e', res.str[1]);
	TEST_ASSERT_EQUAL('l', res.str[2]);
	TEST_ASSERT_EQUAL('l', res.str[3]);
	TEST_ASSERT_EQUAL('o', res.str[4]);
	TEST_ASSERT_EQUAL('z', res.str[5]);
}

void test_string_concat(void) {
	string s;
	TEST_ASSERT_TRUE(string_create(arena, &s, "hello"));

	string s2;
	TEST_ASSERT_TRUE(string_create(arena, &s2, " world"));

	string res;
	TEST_ASSERT_FALSE(string_concat(arena, &res, NULL, &s2));
	TEST_ASSERT_FALSE(string_concat(arena, NULL, &s, &s2));
	TEST_ASSERT_TRUE(string_concat(arena, &res, &s, &s2));
	TEST_ASSERT_EQUAL(11, res.len);
	TEST_ASSERT_EQUAL('w', res.str[6]);
	TEST_ASSERT_EQUAL(0, strcmp(res.str, "hello world"));
}

void test_string_contains_basic(void) {
	string s, s2, no;
	TEST_ASSERT_TRUE(string_create(arena, &s, "hello world"));
	TEST_ASSERT_TRUE(string_create(arena, &s2, " world"));
	TEST_ASSERT_TRUE(string_create(arena, &no, " worlld"));

	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains(&s, NULL));
	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains(NULL, &s2));
	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains_cstr(&s, ""));
	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains(&s, &no));

	TEST_ASSERT_EQUAL(5, string_contains(&s, &s2));
	TEST_ASSERT_EQUAL(2, string_contains_cstr(&s, "llo w"));
}

void test_string_contains_same_str(void) {
	string s;
	TEST_ASSERT_TRUE(string_create(arena, &s, "hello world"));

	string s2;
	TEST_ASSERT_TRUE(string_create(arena, &s2, "hello world"));

	TEST_ASSERT_EQUAL(0, string_contains(&s, &s2));
	TEST_ASSERT_EQUAL(0, string_contains_cstr(&s, "hello world"));
}

void test_string_compare(void) {
	string s, s2, s3, s4;

	TEST_ASSERT_TRUE(string_create(arena, &s, "hello world"));
	TEST_ASSERT_TRUE(string_create(arena, &s2, "hello worl "));
	TEST_ASSERT_TRUE(string_create(arena, &s3, "ello world "));
	TEST_ASSERT_TRUE(string_create(arena, &s4, "hello world"));

	TEST_ASSERT_FALSE(string_compare(&s, &s2));
	TEST_ASSERT_FALSE(string_compare(&s, &s3));
	TEST_ASSERT_FALSE(string_compare(NULL, &s));
	TEST_ASSERT_FALSE(string_compare(&s, NULL));

	// [TODO]:
	// TEST_ASSERT_TRUE(string_compare(&s, &s4));
	// TEST_ASSERT_TRUE(string_compare(&s, &s));
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_string_create);
	RUN_TEST(test_string_copy);
	RUN_TEST(test_string_append);
	RUN_TEST(test_string_concat);
	RUN_TEST(test_string_contains_basic);
	RUN_TEST(test_string_contains_same_str);
	RUN_TEST(test_string_compare);
	return UNITY_END();
}
