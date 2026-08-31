#include "json_internal.h"
#include <filesystem/filesystem.h>
#include <loaders/json_loader.h>
#include <platform/platform.h>

#include <common.h>

#include <string.h>
#include <unity.h>

static arena_t* arena = NULL;

void setUp(void) {
	arena = arena_create(8192);
	TEST_ASSERT_NOT_NULL(arena);
}

void tearDown(void) {
	if (arena) {
		arena_destroy(arena);
		arena = NULL;
	}
}

// void test_json_read_whitespace(void) {
// 	struct test {
// 		char const* txt;
// 		bool should_succeed;
// 		char expect;
// 	};
// 	struct test test_cases[] = {{
// 									.txt = "   \t \n\n \t hello world",
// 									.should_succeed = true,
// 									.expect = 'h',
// 								},
// 								{
// 									.txt = "hi",
// 									.should_succeed = false,
// 								},
// 								{
// 									.txt = "\t\0   word",
// 									.should_succeed = true,
// 									.expect = '\0',
// 								},
// 								{
// 									.txt = "\0",
// 									.should_succeed = false,
// 								}};
//
// 	for (uint32_t i = 0; i < countof(test_cases); ++i) {
// 		struct test test_case = test_cases[i];
// 		if (test_case.should_succeed) {
// 			TEST_ASSERT_TRUE(json_read_whitespace(&test_case.txt));
// 			TEST_ASSERT_EQUAL_CHAR(test_case.expect, *test_case.txt);
// 		} else {
// 			TEST_ASSERT_FALSE(json_read_whitespace(&test_case.txt));
// 		}
// 	}
// }
//
// void test_json_read_bool_good(void) {
// 	bool out = false;
// 	{
// 		char const* true_str = "true";
// 		TEST_ASSERT_TRUE(json_read_bool(&true_str, &out));
// 		TEST_ASSERT_TRUE(out);
// 		TEST_ASSERT_EQUAL_CHAR('\0', *true_str);
// 	}
//
// 	{
// 		char const* false_str =
// 			"false, [1, 8, 3.444, { \"key\": \"value\" } ] } ";
// 		TEST_ASSERT_TRUE(json_read_bool(&false_str, &out));
// 		TEST_ASSERT_FALSE(out);
// 		TEST_ASSERT_EQUAL_CHAR(',', *false_str);
// 	}
//
// 	{
// 		char const* bad_str = " false";
// 		TEST_ASSERT_FALSE(json_read_bool(&bad_str, &out));
// 		TEST_ASSERT_EQUAL_CHAR(' ', *bad_str);
// 	}
// }
//
// void test_json_read_number_good(void) {
// 	struct test {
// 		char const* txt;
// 		bool should_succeed;
// 		double expect;
// 	};
// 	struct test test_cases[] = {
// 		{
// 			.txt = "144",
// 			.should_succeed = true,
// 			.expect = 144.f,
// 		},
// 		{
// 			.txt = "165.2",
// 			.should_succeed = true,
// 			.expect = 165.2f,
// 		},
// 		{
// 			.txt = "0.447",
// 			.should_succeed = true,
// 			.expect = .447f,
// 		},
// 		{
// 			.txt = "144.01",
// 			.should_succeed = true,
// 			.expect = 144.01f,
// 		},
// 		{
// 			.txt = "000140.01001",
// 			.should_succeed = false,
// 		},
// 		{
// 			.txt = ".01001",
// 			.should_succeed = false,
// 		},
// 		{
// 			// TODO: according to spec, this should fail
// 			.txt = "12.",
// 			.should_succeed = true,
// 			.expect = 12.f,
// 		},
// 		{
// 			.txt = "000.140.01001",
// 			.should_succeed = false,
// 		},
// 	};
//
// 	for (uint32_t i = 0; i < countof(test_cases); ++i) {
// 		struct test test_case = test_cases[i];
// 		double out = 0xFFFFFFFF;
// 		if (test_case.should_succeed) {
// 			TEST_ASSERT_TRUE(json_read_number(&test_case.txt, &out));
// 			TEST_ASSERT_EQUAL_FLOAT(test_case.expect, out);
// 		} else {
// 			TEST_ASSERT_FALSE(json_read_number(&test_case.txt, &out));
// 		}
// 	}
// }

void test_json_tokenize_good(void) {
	char const* str = "{ [ \n\t    \t ] } false 123 : ] \"escape me\" \"\"";
	json_lexer lexer = json_lexer_init(str);

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
		TEST_ASSERT_TRUE(json_consume_token(&lexer, &actual_token));
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

	// TODO: should be 2
	TEST_ASSERT_EQUAL(1, lexer.cur_line);
}

void test_json_tokenize_number_good(void) {
	char const* good_str = " 123.0 1203.05 -22.500005 ";
	json_lexer lexer = json_lexer_init(good_str);
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

		TEST_ASSERT_TRUE(json_consume_token(&lexer, &actual_token));
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
	json_lexer lexer = json_lexer_init(str);

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
		TEST_ASSERT_TRUE(json_consume_token(&lexer, &actual_token));
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
	json_lexer bad_lexer = json_lexer_init(bad);
	json_token bad_token = {0};

	TEST_ASSERT_FALSE(json_consume_token(&bad_lexer, &bad_token));
}

void test_json_parse_invalid(void) {

	string invalid_inputs[] = {
		str("{"),
		str("}"),
		str("["),
		str("]"),
		str("\"unclosed string"),
		str("{\"key\":}"),
		str("{\"key\": \"value\""),
		str("{\"key\": \"value\",}"),
		str("{,}"),
		str("{\"a\": 1, \"b\": 2,}"),
		str("[1, 2,]"),
		str("[,]"),
		str("[1,, 2]"),
		str("{: \"value\"}"),
		str("{\"key\" :}"),
		str("\"key\": \"value\""),
		str("{key: \"value\"}"),
		str("{Key: \"value\"}"),
		str("{123: \"value\"}"),
		str("truee"),
		str("TrUe"),
		str("FALSE"),
		str("nulll"),
		str("nil"),
		str("undefined"),
		str("NaN"),
		str("Infinity"),
		str("-Infinity"),
		str("0123"),
		str("00.5"),
		str(".5"),
		str("1..5"),
		str("1e10"),
		str("1E5"),
		str("1.2e3"),
		str("+123"),
		str("+0"),
		str("."),
		str(".."),
		str("1."),
		str("1.2.3"),
		str(""),
		str(" "),
		str("{\"a\" \"b\": \"c\"}"),
		str("{\"a\": \"b\" \"c\": \"d\"}"),
		str("{key: value}"),
		str("{'key': 'value'}"),
		str("[1; 2; 3]"),
		str("{key=\"value\"}"),
		str("[1|2|3]"),
		str("// this is not json"),
		str("# not json either"),
		str("/* comment */ {}"),
		str("{\"a\": true false}"),
		str("null null"),
		str("1 2 3"),
		str("\x01"),
		str("\x1F"),
		str("\x7F"),
		str("\\x41"),
		str("\\z"),
		str("\\"),
		str("\x08\x08"),
		str("[trailing,comma,]"),
		str("{\"nested\": [{\"another\": \"missing close\"}"),
		str("\t{\r\n\"tabs\tand\r\ncrlf\"\r\n}\t\r\n"),
		// str("{\"emoji\": \"😀🎉\"}"), // this is good
	};

	uint64_t base_usage = arena_get_usage(arena);
	for (uint64_t i = 0; i < countof(invalid_inputs); ++i) {
		json_parser parser = json_parser_init(arena, invalid_inputs[i], 100);

		TEST_ASSERT_NULL(json_parser_parse(&parser));
		TEST_ASSERT_EQUAL(base_usage, arena_get_usage(arena));
	}
}

void test_json_parse_valid(void) {
	string const valid_inputs[] = {
		str("0"),
		str("1"),
		str("-1"),
		str("10"),
		str("-10"),
		str("100"),
		str("-100"),
		str("0.5"),
		str("-0.5"),
		str("1.5"),
		str("-1.5"),
		str("3.14"),
		str("-3.14"),
		str("123.456"),
		str("-123.456"),
		str("0.0"),
		str("-0.0"),
		str("1.0"),
		str("0.1"),
		str("0.01"),
		str("true"),
		str("false"),
		str("null"),
		str("\"\""),
		str("\"a\""),
		str("\"abc\""),
		str("\"Hello World\""),
		str("\"a b c\""),
		str("\"quote\\\"inside\""),
		str("\"back\\\\slash\""),
		str("\"tab\\there\""),
		str("\"newline\\nhere\""),
		str("\"carriage\\rreturn\""),
		str("\"form\\ffeed\""),
		str("\"backspace\\bchar\""),
		str("\"forward\\/slash\""),
		str("[]"),
		str("[ ]"),
		str("[1]"),
		str("[1, 2]"),
		str("[1, 2, 3]"),
		str("[true]"),
		str("[false]"),
		str("[null]"),
		str("[\"a\"]"),
		str("[\"a\", \"b\"]"),
		str("[\"a\", \"b\", \"c\"]"),
		str("[true, false, null]"),
		str("[1, true, \"str\", null]"),
		str("[[1]]"),
		str("[[1, 2], [3, 4]]"),
		str("[[]]"),
		str("[[[]]]"),
		str("{}"),
		str("{ }"),
		str("{\"a\": 1}"),
		str("{\"a\": \"b\"}"),
		str("{\"a\": true}"),
		str("{\"a\": false}"),
		str("{\"a\": null}"),
		str("{\"a\": 1, \"b\": 2}"),
		str("{\"a\": 1, \"b\": 2, \"c\": 3}"),
		str("{\"a\": \"b\", \"c\": \"d\"}"),
		str("{\"a\": [1, 2, 3]}"),
		str("{\"a\": {\"b\": 1}}"),
		str("{\"a\": {\"b\": {\"c\": 1}}}"),
		str("{\"a\": [], \"b\": {}}"),
		str("[{\"a\": 1}]"),
		str("[{\"a\": 1}, {\"b\": 2}]"),
		str("{\"a\": [{\"b\": [{\"c\": 1}]}]}"),
		str("[\"\", \" \", \"  \"]"),
		str("{\"k1\": \"v1\", \"k2\": \"v2\"}"),
		str("{\"empty_arr\": [], \"empty_obj\": {}}"),
		str("[1, [2, [3, [4, [5]]]]]"),
		str("{\"nested\": {\"deep\": {\"deeper\": {\"deepest\": \"val\"}}}}"),
		str("[true, true, true]"),
		str("[null, null, null]"),
		str("{\"\": \"empty key\"}"),
		str("{\" \": \"space key\"}"),
		str("[[[[[1]]]]]"),
		str("{\"a\": 1, \"b\": [1, 2], \"c\": {\"d\": true}}"),
		str("[null]"),
		str("{\"num\": 0, \"neg\": -1, \"float\": 1.5, \"bool\": true, "
			"\"str\": "
			"\"hi\"}"),
	};

	for (uint64_t i = 0; i < countof(valid_inputs); ++i) {
		json_parser parser = json_parser_init(arena, valid_inputs[i], 100);

		TEST_ASSERT_NOT_NULL(json_parser_parse(&parser));

		json_parser_free(&parser);
	}
}

void test_json_parse_nodes(void) {
	string json_content = str("[ 123, 456, 789, { \"key\": 999 }, {\n} ]");
	json_parser parser = json_parser_init(arena, json_content, 100);
	json_node* node = json_parser_parse(&parser);
	TEST_ASSERT_NOT_NULL(node);

	TEST_ASSERT_EQUAL(JSON_VALUE_ARRAY, node->value_type);
	TEST_ASSERT_EQUAL(5, node->value.array.size);

	json_array_elem* elem = node->value.array.first;

	for (uint32_t i = 0; i < 3; i++) {
		TEST_ASSERT_EQUAL(JSON_VALUE_NUMBER, elem->node->value_type);
		elem = elem->next;
	}

	TEST_ASSERT_EQUAL(JSON_VALUE_OBJECT, elem->node->value_type);
	TEST_ASSERT_EQUAL(3, elem->index);

	TEST_ASSERT_EQUAL(1, elem->node->value.object.size);
	json_object_elem* obj = elem->node->value.object.first;
	TEST_ASSERT_NULL(obj->next);

	TEST_ASSERT_TRUE(string_compare(obj->key, str("key")));
	TEST_ASSERT_EQUAL(JSON_VALUE_NUMBER, obj->value->value_type);
	TEST_ASSERT_EQUAL(999.f, obj->value->value.number);

	elem = elem->next;
	TEST_ASSERT_NULL(elem->next);
	TEST_ASSERT_EQUAL(JSON_VALUE_OBJECT, elem->node->value_type);
	TEST_ASSERT_EQUAL(0, elem->node->value.object.size);
	TEST_ASSERT_NULL(elem->node->value.object.first);
}

void test_json_max_depth(void) {
	string json_content = str("[ { \"key\": [ [ 1, 2, 3 ] ] } ]");
	{
		json_parser parser = json_parser_init(arena, json_content, 5);
		json_node* node = json_parser_parse(&parser);
		TEST_ASSERT_NOT_NULL(node);
	}
	{
		json_parser parser = json_parser_init(arena, json_content, 4);
		json_node* node = json_parser_parse(&parser);
		TEST_ASSERT_NULL(node);
	}
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_json_tokenize_good);
	RUN_TEST(test_json_tokenize_number_good);
	RUN_TEST(test_json_tokenize_number_bad);
	RUN_TEST(test_json_tokenize_keywords);
	RUN_TEST(test_json_parse_invalid);
	RUN_TEST(test_json_parse_valid);
	RUN_TEST(test_json_parse_nodes);
	RUN_TEST(test_json_max_depth);

	return UNITY_END();
}
