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

void test_json_read_whitespace(void) {
	struct test {
		char const* txt;
		bool should_succeed;
		char expect;
	};
	struct test test_cases[] = {{
									.txt = "   \t \n\n \t hello world",
									.should_succeed = true,
									.expect = 'h',
								},
								{
									.txt = "hi",
									.should_succeed = false,
								},
								{
									.txt = "\t\0   word",
									.should_succeed = true,
									.expect = '\0',
								},
								{
									.txt = "\0",
									.should_succeed = false,
								}};

	for (uint32_t i = 0; i < countof(test_cases); ++i) {
		struct test test_case = test_cases[i];
		if (test_case.should_succeed) {
			TEST_ASSERT_TRUE(json_read_whitespace(&test_case.txt));
			TEST_ASSERT_EQUAL_CHAR(test_case.expect, *test_case.txt);
		} else {
			TEST_ASSERT_FALSE(json_read_whitespace(&test_case.txt));
		}
	}
}

void test_json_read_bool_good(void) {
	bool out = false;
	{
		char const* true_str = "true";
		TEST_ASSERT_TRUE(json_read_bool(&true_str, &out));
		TEST_ASSERT_TRUE(out);
		TEST_ASSERT_EQUAL_CHAR('\0', *true_str);
	}

	{
		char const* false_str =
			"false, [1, 8, 3.444, { \"key\": \"value\" } ] } ";
		TEST_ASSERT_TRUE(json_read_bool(&false_str, &out));
		TEST_ASSERT_FALSE(out);
		TEST_ASSERT_EQUAL_CHAR(',', *false_str);
	}

	{
		char const* bad_str = " false";
		TEST_ASSERT_FALSE(json_read_bool(&bad_str, &out));
		TEST_ASSERT_EQUAL_CHAR(' ', *bad_str);
	}
}

void test_json_read_number_good(void) {
	struct test {
		char const* txt;
		bool should_succeed;
		double expect;
	};
	struct test test_cases[] = {
		{
			.txt = "144",
			.should_succeed = true,
			.expect = 144.f,
		},
		{
			.txt = "165.2",
			.should_succeed = true,
			.expect = 165.2f,
		},
		{
			.txt = "0.447",
			.should_succeed = true,
			.expect = .447f,
		},
		{
			.txt = "144.01",
			.should_succeed = true,
			.expect = 144.01f,
		},
		{
			.txt = "000140.01001",
			.should_succeed = false,
		},
		{
			.txt = ".01001",
			.should_succeed = false,
		},
		{
			// TODO: according to spec, this should fail
			.txt = "12.",
			.should_succeed = true,
			.expect = 12.f,
		},
		{
			.txt = "000.140.01001",
			.should_succeed = false,
		},
	};

	for (uint32_t i = 0; i < countof(test_cases); ++i) {
		struct test test_case = test_cases[i];
		double out = 0xFFFFFFFF;
		if (test_case.should_succeed) {
			TEST_ASSERT_TRUE(json_read_number(&test_case.txt, &out));
			TEST_ASSERT_EQUAL_FLOAT(test_case.expect, out);
		} else {
			TEST_ASSERT_FALSE(json_read_number(&test_case.txt, &out));
		}
	}
}

void test_json_tokenize(void) {
	char const* str = "{ [ \n\t    \t ] } false 123";
	json_lexer lexer = {
		.base_str = str,
		.str = str,
		.cur_line = 0,
	};
	json_token token = {0};

	// open bracket
	TEST_ASSERT_TRUE(json_read_token(&lexer, &token));
	TEST_ASSERT_EQUAL(JSON_TOKEN_OPEN_BRACKET, token.type);
	TEST_ASSERT_EQUAL_CHAR('{', token.value.chr);
	TEST_ASSERT_EQUAL(1, token.length);
	TEST_ASSERT_EQUAL(0, token.begin_pos);

	// open square
	TEST_ASSERT_TRUE(json_read_token(&lexer, &token));
	TEST_ASSERT_EQUAL(JSON_TOKEN_OPEN_SQ_BRACKET, token.type);
	TEST_ASSERT_EQUAL_CHAR('[', token.value.chr);
	TEST_ASSERT_EQUAL(1, token.length);
	TEST_ASSERT_EQUAL(2, token.begin_pos);

	// close square
	TEST_ASSERT_TRUE(json_read_token(&lexer, &token));
	TEST_ASSERT_EQUAL(JSON_TOKEN_CLOSE_SQ_BRACKET, token.type);
	TEST_ASSERT_EQUAL_CHAR(']', token.value.chr);
	TEST_ASSERT_EQUAL(1, token.length);
	TEST_ASSERT_EQUAL(12, token.begin_pos);

	// close bracket
	TEST_ASSERT_TRUE(json_read_token(&lexer, &token));
	TEST_ASSERT_EQUAL(JSON_TOKEN_CLOSE_BRACKET, token.type);
	TEST_ASSERT_EQUAL_CHAR('}', token.value.chr);
	TEST_ASSERT_EQUAL(1, token.length);
	TEST_ASSERT_EQUAL(14, token.begin_pos);

	// false
	TEST_ASSERT_TRUE(json_read_token(&lexer, &token));
	TEST_ASSERT_EQUAL(JSON_TOKEN_KEYWORD, token.type);
	TEST_ASSERT_EQUAL(5, token.length);
	TEST_ASSERT_EQUAL(16, token.begin_pos);

	// number
	TEST_ASSERT_TRUE(json_read_token(&lexer, &token));
	TEST_ASSERT_EQUAL(JSON_TOKEN_NUMBER, token.type);
	TEST_ASSERT_EQUAL(3, token.length);
	TEST_ASSERT_EQUAL(22, token.begin_pos);

	TEST_ASSERT_EQUAL(1, lexer.cur_line);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_json_read_whitespace);
	RUN_TEST(test_json_read_bool_good);
	RUN_TEST(test_json_read_number_good);
	RUN_TEST(test_json_tokenize);

	return UNITY_END();
}
