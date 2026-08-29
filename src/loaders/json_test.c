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

void test_json_tokenize_good(void) {
	char const* str = "{ [ \n\t    \t ] } false 123 : ] \"escape me\" \"\"";
	json_lexer lexer = {
		.base_str = str,
		.str = str,
		.cur_line = 0,
	};

	json_token expected_tokens[] = {
		{
			.type = JSON_TOKEN_OPEN_BRACKET,
			.value.chr = '{',
			.length = 1,
			.begin_pos = 0,
		},
		{
			.type = JSON_TOKEN_OPEN_SQ_BRACKET,
			.value.chr = '[',
			.length = 1,
			.begin_pos = 2,
		},
		{
			.type = JSON_TOKEN_CLOSE_SQ_BRACKET,
			.value.chr = ']',
			.length = 1,
			.begin_pos = 12,
		},
		{
			.type = JSON_TOKEN_CLOSE_BRACKET,
			.value.chr = '}',
			.length = 1,
			.begin_pos = 14,
		},
		{
			.type = JSON_TOKEN_KEYWORD,
			.length = 5,
			.begin_pos = 16,
			.value.s = str("false"),
		},
		{
			.type = JSON_TOKEN_NUMBER,
			.length = 3,
			.begin_pos = 22,
			.value.s = str("123"),
		},
		{
			.type = JSON_TOKEN_COLON,
			.value.chr = ':',
			.length = 1,
			.begin_pos = 26,
		},
		{
			.type = JSON_TOKEN_CLOSE_SQ_BRACKET,
			.value.chr = ']',
			.length = 1,
			.begin_pos = 28,
		},
		{
			.type = JSON_TOKEN_STRING,
			.length = 9,
			.begin_pos = 30,
			.value.s = str("escape me"),
		},
		{
			.type = JSON_TOKEN_STRING,
			.length = 0,
			.begin_pos = 42,
			.value.s = STRING_EMPTY,
		},
		{
			.type = JSON_TOKEN_EOF,
			.length = 0,
			.begin_pos = 44,
			.value.chr = '\0',
		},
	};

	for (uint32_t i = 0; i < countof(expected_tokens); i++) {
		json_token expected_token = expected_tokens[i];
		json_token actual_token = {0};
		TEST_ASSERT_TRUE(json_read_token(&lexer, &actual_token));
		TEST_ASSERT_EQUAL(expected_token.type, actual_token.type);
		TEST_ASSERT_EQUAL(expected_token.begin_pos, actual_token.begin_pos);
		TEST_ASSERT_EQUAL(expected_token.length, actual_token.length);
		if (expected_token.type <= JSON_TOKEN_CLOSE_SQ_BRACKET) {
			TEST_ASSERT_EQUAL_CHAR(expected_token.value.chr,
								   actual_token.value.chr);
		} else {
			TEST_ASSERT_TRUE(
				string_compare(expected_token.value.s, actual_token.value.s));
		}
	}

	TEST_ASSERT_EQUAL(1, lexer.cur_line);
}

void test_json_tokenize_number_good(void) {
	char const* good_str = " 123.0 1203.05 -22.500005 ";
	json_lexer lexer = {
		.base_str = good_str,
		.str = good_str,
		.cur_line = 0,
	};
	json_token expected_tokens[] = {
		{
			.type = JSON_TOKEN_NUMBER,
			.length = 5,
			.begin_pos = 1,
			.value.s = str("123.0"),
		},
		{
			.type = JSON_TOKEN_NUMBER,
			.length = 7,
			.begin_pos = 7,
			.value.s = str("1203.05"),
		},
		{
			.type = JSON_TOKEN_NUMBER,
			.length = 10,
			.begin_pos = 15,
			.value.s = str("-22.500005"),
		},
	};

	for (uint32_t i = 0; i < countof(expected_tokens); i++) {
		json_token expected_token = expected_tokens[i];
		json_token actual_token = {0};

		TEST_ASSERT_TRUE(json_read_token(&lexer, &actual_token));
		TEST_ASSERT_EQUAL(expected_token.type, actual_token.type);
		TEST_ASSERT_EQUAL(expected_token.begin_pos, actual_token.begin_pos);
		TEST_ASSERT_EQUAL(expected_token.length, actual_token.length);
		TEST_ASSERT_TRUE(
			string_compare(expected_token.value.s, actual_token.value.s));
	}
}

void test_json_tokenize_number_bad(void) {
	struct test_case {
		char const* name;
		char const* str;
	};

	const struct test_case test_cases[] = {
		{
			.name = "leading space",
			.str = " -10",
		},
		{
			.name = "double minus sign",
			.str = "--10",
		},
		{
			.name = "minus sign after dot",
			.str = "4.-23",
		},
		{
			.name = "leading 0",
			.str = "010",
		},
		{
			.name = "leading 0 with minus",
			.str = "-010",
		},
		{
			.name = "leading .",
			.str = ".12",
		},
		{
			.name = "trailing .",
			.str = "12.",
		},
	};

	for (uint32_t i = 0; i < countof(test_cases); ++i) {
		json_token token = {0};
		struct test_case tcase = test_cases[i];

		TEST_ASSERT_FALSE_MESSAGE(json_lex_number(tcase.str, &token),
								  tcase.name);
	}
}

void test_json_tokenize_keywords(void) {
	char const* str = "false true null bad";
	json_lexer lexer = {
		.base_str = str,
		.str = str,
		.cur_line = 0,
	};

	json_token expected_tokens[] = {
		{
			.type = JSON_TOKEN_KEYWORD,
			.length = 5,
			.begin_pos = 0,
			.value.s = str("false"),
		},
		{
			.type = JSON_TOKEN_KEYWORD,
			.length = 4,
			.begin_pos = 6,
			.value.s = str("true"),
		},
		{
			.type = JSON_TOKEN_KEYWORD,
			.length = 4,
			.begin_pos = 11,
			.value.s = str("null"),
		},
		{
			.type = JSON_TOKEN_KEYWORD,
			.length = 3,
			.begin_pos = 16,
			.value.s = str("bad"),
		},
		{
			.type = JSON_TOKEN_EOF,
			.length = 0,
			.begin_pos = 19,
			.value.chr = '\0',
		},
	};

	for (uint32_t i = 0; i < countof(expected_tokens); i++) {
		json_token expected_token = expected_tokens[i];
		json_token actual_token = {0};
		TEST_ASSERT_TRUE(json_read_token(&lexer, &actual_token));
		TEST_ASSERT_EQUAL(expected_token.type, actual_token.type);
		TEST_ASSERT_EQUAL(expected_token.begin_pos, actual_token.begin_pos);
		TEST_ASSERT_EQUAL(expected_token.length, actual_token.length);
		if (expected_token.type <= JSON_TOKEN_CLOSE_SQ_BRACKET) {
			TEST_ASSERT_EQUAL_CHAR(expected_token.value.chr,
								   actual_token.value.chr);
		} else {
			TEST_ASSERT_TRUE(
				string_compare(expected_token.value.s, actual_token.value.s));
		}
	}
	TEST_ASSERT_EQUAL(0, lexer.cur_line);

	// --- Keyword is too long --- //
	char const* bad = "truenull";
	json_lexer bad_lexer = {
		.base_str = bad,
		.str = bad,
		.cur_line = 0,
	};
	json_token bad_token = {0};

	TEST_ASSERT_FALSE(json_read_token(&bad_lexer, &bad_token));
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_json_read_whitespace);
	RUN_TEST(test_json_read_bool_good);
	RUN_TEST(test_json_read_number_good);
	RUN_TEST(test_json_tokenize_good);
	RUN_TEST(test_json_tokenize_number_good);
	RUN_TEST(test_json_tokenize_number_bad);
	RUN_TEST(test_json_tokenize_keywords);

	return UNITY_END();
}
