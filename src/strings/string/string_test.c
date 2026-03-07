#include <allocators/arena.h>
#include <containers/array_types.h>
#include <core/platform.h>
#include <string.h>
#include <strings/string.h>
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

void test_string_create_fmt(void) {
	string s;
	TEST_ASSERT_TRUE(string_create(arena, &s, "Hello -42 World"));
	TEST_ASSERT_EQUAL(15, s.len);

	string fmt;
	TEST_ASSERT_TRUE(
		string_create_fmt(arena, &fmt, "Hello %d %s", -42, "World"));
	TEST_ASSERT_EQUAL(s.len, fmt.len);
	TEST_ASSERT_NOT_NULL(fmt.str);

	TEST_ASSERT_TRUE(string_compare(&s, &fmt));
}

void test_string_array(void) {
	size_t count = 8;
	string_array arr = string_array_create(arena, count);
	TEST_ASSERT_NOT_NULL(arr.raw);
	TEST_ASSERT_EQUAL(count, string_array_size(arr));
}

void test_string_format_args(void) {
	char buf[256];

	{
		char const* expect = "Hello World!";
		string s;
		TEST_ASSERT_TRUE(string_create(arena, &s, "World"));

		sprintf(buf, "Hello " STR_FMT "!", STR_FMT_ARGS(s));

		TEST_ASSERT_EQUAL(0, strncmp(buf, expect, 12));
	}

	memset(buf, 0, 256);

	{ // Harder one
		char const* expect = "Hello 42.88 World!";
		string hello;
		TEST_ASSERT_TRUE(string_create(arena, &hello, "Hello"));
		string world;
		TEST_ASSERT_TRUE(string_create_fmt(arena, &world, "%.2f World", 42.88));

		sprintf(buf, STR_FMT " " STR_FMT "!", STR_FMT_ARGS(hello),
				STR_FMT_ARGS(world));

		TEST_ASSERT_EQUAL(0, strncmp(buf, expect, strlen(expect)));
	}
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

void test_string_contains_last_basic(void) {
	string s, s2, no;
	TEST_ASSERT_TRUE(string_create(arena, &s, "hello world"));
	TEST_ASSERT_TRUE(string_create(arena, &s2, " world"));
	TEST_ASSERT_TRUE(string_create(arena, &no, " worlld"));

	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains_last(&s, NULL));
	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains_last(NULL, &s2));
	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains_last_cstr(&s, ""));
	TEST_ASSERT_EQUAL(STRING_NPOS, string_contains_last(&s, &no));

	TEST_ASSERT_EQUAL(5, string_contains_last(&s, &s2));
	TEST_ASSERT_EQUAL(2, string_contains_last_cstr(&s, "llo w"));
}

void test_string_contains_last(void) {
	{
		string s, s2;
		TEST_ASSERT_TRUE(string_create(arena, &s, "hello world hello world"));
		TEST_ASSERT_TRUE(string_create(arena, &s2, " world"));

		TEST_ASSERT_EQUAL(17, string_contains_last(&s, &s2));
		TEST_ASSERT_EQUAL(14, string_contains_last_cstr(&s, "llo w"));
	}
	{
		string s, s2;
		TEST_ASSERT_TRUE(string_create(arena, &s, "ababababa"));
		TEST_ASSERT_TRUE(string_create(arena, &s2, "ab"));

		TEST_ASSERT_EQUAL(6, string_contains_last(&s, &s2));
		TEST_ASSERT_EQUAL(6, string_contains_last_cstr(&s, "ab"));
	}
	{
		// Only occurence is at position 0
		string s, s2;
		TEST_ASSERT_TRUE(string_create(arena, &s, "abfoobarbaz"));
		TEST_ASSERT_TRUE(string_create(arena, &s2, "ab"));

		TEST_ASSERT_EQUAL(0, string_contains_last(&s, &s2));
		TEST_ASSERT_EQUAL(0, string_contains_last_cstr(&s, "ab"));
	}
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

	TEST_ASSERT_TRUE(string_compare(&s, &s4));
	TEST_ASSERT_TRUE(string_compare(&s, &s));
}

void test_string_occurences(void) {
	typedef struct test {
		char const* cstr;
		char const* cpattern;
		size_t occurences_count;
		size_t* occurences;
	} test;

	size_t t0[2] = {3, 8};
	size_t t1[6] = {0, 5, 8, 10, 19, 20};
	size_t t4[3] = {3, 8, 13};
	test tests[5] = {
		{
			.cstr = "abc__def__hij",
			.cpattern = "__",
			.occurences_count = 2,
			.occurences = t0,
		},
		{
			.cstr = " this is a sentence  cool",
			.cpattern = " ",
			.occurences_count = 6,
			.occurences = t1,
		},
		{
			.cstr = "aa",
			.cpattern = "abc",
			.occurences_count = 0,
			.occurences = NULL,
		},
		{
			.cstr = "abc__def__hij",
			.cpattern = "___",
			.occurences_count = 0,
			.occurences = NULL,
		},
		{
			.cstr = "abc_,def_,hij_,",
			.cpattern = "_,",
			.occurences_count = 3,
			.occurences = t4,
		},
	};

	for (size_t i = 0; i < 5; ++i) {
		test t = tests[i];
		u64_array expect;
		if (t.occurences_count) {
			expect = u64_array_create(arena, t.occurences_count);
			TEST_ASSERT_NOT_NULL(expect.raw);
		}

		for (size_t j = 0; j < t.occurences_count; j++) {
			size_t val = t.occurences[j];
			TEST_ASSERT_TRUE(u64_array_set(expect, j, &val));
		}

		string str;
		TEST_ASSERT_TRUE(string_create(arena, &str, t.cstr));
		string pattern;
		TEST_ASSERT_TRUE(string_create(arena, &pattern, t.cpattern));

		u64_array ret;
		if (t.occurences_count) {
			TEST_ASSERT_TRUE(string_occurences(arena, &ret, &str, &pattern));
			TEST_ASSERT_EQUAL(t.occurences_count, u64_array_size(ret));
		} else {
			TEST_ASSERT_FALSE(string_occurences(arena, &ret, &str, &pattern));
		}

		for (size_t j = 0; j < t.occurences_count; j++) {
			size_t got, expected;
			TEST_ASSERT_TRUE(u64_array_get(expect, &expected, j));
			TEST_ASSERT_TRUE(u64_array_get(ret, &got, j));
			TEST_ASSERT_EQUAL(expected, got);
		}
		arena_reset(arena);
	}
}

void test_string_tokenize_regular_trail(void) {
#define TEST_COUNT 2
	char const* cstrs[TEST_COUNT] = {
		"abc__def__hij",   // regular
		"abc__def__hij__", // trailing
	};

	for (size_t i = 0; i < TEST_COUNT; i++) {
		string_array expect = string_array_create(arena, 3);
		TEST_ASSERT_NOT_NULL(expect.raw);

		string full;
		TEST_ASSERT_TRUE(string_create(arena, &full, cstrs[i]));
		string pattern;
		TEST_ASSERT_TRUE(string_create(arena, &pattern, "__"));

		string* s;
		TEST_ASSERT_TRUE(string_array_at(expect, &s, 0));
		s->len = 3;
		s->str = &full.str[0];
		TEST_ASSERT_TRUE(string_array_at(expect, &s, 1));
		s->len = 3;
		s->str = &full.str[5];
		TEST_ASSERT_TRUE(string_array_at(expect, &s, 2));
		s->len = 3;
		s->str = &full.str[10];

		string_array ret;
		TEST_ASSERT_TRUE(string_tokenize(arena, &ret, &full, &pattern));
		TEST_ASSERT_NOT_NULL(ret.raw);
		TEST_ASSERT_EQUAL(string_array_size(expect), string_array_size(ret));

		string *expect_begin, *ret_begin;
		TEST_ASSERT_TRUE(string_array_at(expect, &expect_begin, 0));
		TEST_ASSERT_TRUE(string_array_at(ret, &ret_begin, 0));

		TEST_ASSERT_EQUAL(0,
						  memcmp(expect_begin, ret_begin, 3 * sizeof(string)));
		TEST_ASSERT_EQUAL(3, ret_begin[0].len);
		TEST_ASSERT_EQUAL(3, ret_begin[1].len);
		TEST_ASSERT_EQUAL(3, ret_begin[2].len);
		TEST_ASSERT_EQUAL(expect_begin[0].str, ret_begin[0].str);
		TEST_ASSERT_EQUAL(expect_begin[1].str, ret_begin[1].str);
		TEST_ASSERT_EQUAL(expect_begin[2].str, ret_begin[2].str);

		arena_reset(arena);
	}
#undef TEST_COUNT
}

void test_string_tokenize_lead(void) {
	char const* cstrs = "__abc__def__hij";

	string_array expect = string_array_create(arena, 3);
	TEST_ASSERT_NOT_NULL(expect.raw);

	string full;
	TEST_ASSERT_TRUE(string_create(arena, &full, cstrs));
	string pattern;
	TEST_ASSERT_TRUE(string_create(arena, &pattern, "__"));

	string* s;
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 0));
	s->len = 3;
	s->str = &full.str[2];
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 1));
	s->len = 3;
	s->str = &full.str[7];
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 2));
	s->len = 3;
	s->str = &full.str[12];

	string_array ret;
	TEST_ASSERT_TRUE(string_tokenize(arena, &ret, &full, &pattern));
	TEST_ASSERT_NOT_NULL(ret.raw);
	TEST_ASSERT_EQUAL(string_array_size(expect), string_array_size(ret));

	string *expect_begin, *ret_begin;
	TEST_ASSERT_TRUE(string_array_at(expect, &expect_begin, 0));
	TEST_ASSERT_TRUE(string_array_at(ret, &ret_begin, 0));

	TEST_ASSERT_EQUAL(0, memcmp(expect_begin, ret_begin, 3 * sizeof(string)));
	TEST_ASSERT_EQUAL(3, ret_begin[0].len);
	TEST_ASSERT_EQUAL(3, ret_begin[1].len);
	TEST_ASSERT_EQUAL(3, ret_begin[2].len);
	TEST_ASSERT_EQUAL(expect_begin[0].str, ret_begin[0].str);
	TEST_ASSERT_EQUAL(expect_begin[1].str, ret_begin[1].str);
	TEST_ASSERT_EQUAL(expect_begin[2].str, ret_begin[2].str);

	arena_reset(arena);
}

void test_string_tokenize_variable_length(void) {
	char const* cstr = "/home/myuser/Code/c/project";

	string_array expect = string_array_create(arena, 5);
	TEST_ASSERT_NOT_NULL(expect.raw);

	string full;
	TEST_ASSERT_TRUE(string_create(arena, &full, cstr));
	string pattern;
	TEST_ASSERT_TRUE(string_create(arena, &pattern, "/"));

	string* s;
	// home
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 0));
	s->len = 4;
	s->str = &full.str[1];
	// myuser
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 1));
	s->len = 6;
	s->str = &full.str[6];
	// Code
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 2));
	s->len = 4;
	s->str = &full.str[13];
	// c
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 3));
	s->len = 1;
	s->str = &full.str[18];
	// project
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 4));
	s->len = 7;
	s->str = &full.str[20];

	// Actual test
	string_array ret;
	TEST_ASSERT_TRUE(string_tokenize(arena, &ret, &full, &pattern));
	TEST_ASSERT_NOT_NULL(ret.raw);
	TEST_ASSERT_EQUAL(string_array_size(expect), string_array_size(ret));

	string *expect_begin, *ret_begin;
	TEST_ASSERT_TRUE(string_array_at(expect, &expect_begin, 0));
	TEST_ASSERT_TRUE(string_array_at(ret, &ret_begin, 0));

	TEST_ASSERT_EQUAL(0, memcmp(expect_begin, ret_begin, 5 * sizeof(string)));
	TEST_ASSERT_EQUAL(4, ret_begin[0].len);
	TEST_ASSERT_EQUAL(6, ret_begin[1].len);
	TEST_ASSERT_EQUAL(4, ret_begin[2].len);
	TEST_ASSERT_EQUAL(1, ret_begin[3].len);
	TEST_ASSERT_EQUAL(7, ret_begin[4].len);
	TEST_ASSERT_EQUAL(expect_begin[0].str, ret_begin[0].str);
	TEST_ASSERT_EQUAL(expect_begin[1].str, ret_begin[1].str);
	TEST_ASSERT_EQUAL(expect_begin[2].str, ret_begin[2].str);
	TEST_ASSERT_EQUAL(expect_begin[3].str, ret_begin[3].str);
	TEST_ASSERT_EQUAL(expect_begin[4].str, ret_begin[4].str);
}

void test_string_tokenize_nomatch(void) {
#define TEST_COUNT 3
	char const* cstrs[TEST_COUNT] = {"abc", "_a_b_c_", "abcd"};
	char const* cpatterns[TEST_COUNT] = {
		"abc",
		"c__",
		"abcd",
	};

	for (size_t i = 0; i < TEST_COUNT; i++) {
		string str, pattern;
		TEST_ASSERT_TRUE(string_create(arena, &str, cstrs[i]));
		TEST_ASSERT_TRUE(string_create(arena, &pattern, cpatterns[i]));
		string_array ret = {.raw = NULL};
		TEST_ASSERT_FALSE(string_tokenize(arena, &ret, &str, &pattern));
		TEST_ASSERT_NULL(ret.raw);
	}
#undef TEST_COUNT
}

// Multiple stacked patterns returns empty strings with NULL contents.
void test_string_tokenize_close_patterns(void) {
	char const* cstr = "a,,b,c,d";
	char const* cpattern = ",";
	// expect: ['a' - '' - 'b' - 'c' - 'd']

	string str, pattern;
	TEST_ASSERT_TRUE(string_create(arena, &str, cstr));
	TEST_ASSERT_TRUE(string_create(arena, &pattern, cpattern));
	string_array expect = string_array_create(arena, 5);
	TEST_ASSERT_NOT_NULL(expect.raw);

	string* s;
	// a
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 0));
	s->len = 1;
	s->str = &str.str[0];
	// (empty)
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 1));
	s->len = 0;
	s->str = NULL;
	// b
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 2));
	s->len = 1;
	s->str = &str.str[3];
	// c
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 3));
	s->len = 1;
	s->str = &str.str[5];
	// d
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 4));
	s->len = 1;
	s->str = &str.str[7];

	// Actual test
	string_array ret;
	TEST_ASSERT_TRUE(string_tokenize(arena, &ret, &str, &pattern));
	TEST_ASSERT_NOT_NULL(ret.raw);
	TEST_ASSERT_EQUAL(string_array_size(expect), string_array_size(ret));

	string *expect_begin, *ret_begin;
	TEST_ASSERT_TRUE(string_array_at(expect, &expect_begin, 0));
	TEST_ASSERT_TRUE(string_array_at(ret, &ret_begin, 0));

	TEST_ASSERT_EQUAL(0, memcmp(expect_begin, ret_begin, 5 * sizeof(string)));
	TEST_ASSERT_EQUAL(expect_begin[0].len, ret_begin[0].len);
	TEST_ASSERT_EQUAL(0, ret_begin[1].len);
	TEST_ASSERT_EQUAL(expect_begin[2].len, ret_begin[2].len);
	TEST_ASSERT_EQUAL(expect_begin[3].len, ret_begin[3].len);
	TEST_ASSERT_EQUAL(expect_begin[4].len, ret_begin[4].len);

	TEST_ASSERT_EQUAL(expect_begin[0].str, ret_begin[0].str);
	TEST_ASSERT_NULL(ret_begin[1].str);
	TEST_ASSERT_EQUAL(expect_begin[2].str, ret_begin[2].str);
	TEST_ASSERT_EQUAL(expect_begin[3].str, ret_begin[3].str);
	TEST_ASSERT_EQUAL(expect_begin[4].str, ret_begin[4].str);
}

void test_string_tokenize_close_patterns_many(void) {
	char const* cstr = "a,,,,b,,,c,,d";
	char const* cpattern = ",,";
	// expect: ['a' - '' - 'b' - ',c' - 'd']

	string str, pattern;
	TEST_ASSERT_TRUE(string_create(arena, &str, cstr));
	TEST_ASSERT_TRUE(string_create(arena, &pattern, cpattern));
	string_array expect = string_array_create(arena, 5);
	TEST_ASSERT_NOT_NULL(expect.raw);

	string* s;
	// a
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 0));
	s->len = 1;
	s->str = &str.str[0];
	// (empty)
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 1));
	s->len = 0;
	s->str = NULL;
	// b
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 2));
	s->len = 1;
	s->str = &str.str[5];
	// ,c
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 3));
	s->len = 2;
	s->str = &str.str[8];
	// d
	TEST_ASSERT_TRUE(string_array_at(expect, &s, 4));
	s->len = 1;
	s->str = &str.str[12];

	// Actual test
	string_array ret;
	TEST_ASSERT_TRUE(string_tokenize(arena, &ret, &str, &pattern));
	TEST_ASSERT_NOT_NULL(ret.raw);
	TEST_ASSERT_EQUAL(string_array_size(expect), string_array_size(ret));

	string *expect_begin, *ret_begin;
	TEST_ASSERT_TRUE(string_array_at(expect, &expect_begin, 0));
	TEST_ASSERT_TRUE(string_array_at(ret, &ret_begin, 0));

	TEST_ASSERT_EQUAL(0, memcmp(expect_begin, ret_begin, 5 * sizeof(string)));
	TEST_ASSERT_EQUAL(expect_begin[0].len, ret_begin[0].len);
	TEST_ASSERT_EQUAL(0, ret_begin[1].len);
	TEST_ASSERT_EQUAL(expect_begin[2].len, ret_begin[2].len);
	TEST_ASSERT_EQUAL(expect_begin[3].len, ret_begin[3].len);
	TEST_ASSERT_EQUAL(expect_begin[4].len, ret_begin[4].len);

	TEST_ASSERT_EQUAL(expect_begin[0].str, ret_begin[0].str);
	TEST_ASSERT_NULL(ret_begin[1].str);
	TEST_ASSERT_EQUAL(expect_begin[2].str, ret_begin[2].str);
	TEST_ASSERT_EQUAL(expect_begin[3].str, ret_begin[3].str);
	TEST_ASSERT_EQUAL(expect_begin[4].str, ret_begin[4].str);
}

void test_string_tokenize_only_pattern(void) {
	char const* cstr = "----";
	char const* cpattern = "-";
	// expect: 3 empty strings.

	string str, pattern;
	TEST_ASSERT_TRUE(string_create(arena, &str, cstr));
	TEST_ASSERT_TRUE(string_create(arena, &pattern, cpattern));
	string_array expect = string_array_create(arena, 3);
	TEST_ASSERT_NOT_NULL(expect.raw);

	string* s;
	for (size_t i = 0; i < 3; ++i) {
		TEST_ASSERT_TRUE(string_array_at(expect, &s, i));
		s->len = 0;
		s->str = NULL;
	}

	// Actual test
	string_array ret;
	TEST_ASSERT_TRUE(string_tokenize(arena, &ret, &str, &pattern));
	TEST_ASSERT_NOT_NULL(ret.raw);
	TEST_ASSERT_EQUAL(string_array_size(expect), string_array_size(ret));

	string *expect_begin, *ret_begin;
	TEST_ASSERT_TRUE(string_array_at(expect, &expect_begin, 0));
	TEST_ASSERT_TRUE(string_array_at(ret, &ret_begin, 0));
	TEST_ASSERT_EQUAL(0, memcmp(expect_begin, ret_begin, 3 * sizeof(string)));

	for (size_t i = 0; i < 3; ++i) {
		TEST_ASSERT_EQUAL(expect_begin[i].len, ret_begin[i].len);
		TEST_ASSERT_EQUAL(expect_begin[i].str, ret_begin[i].str);
	}
}

void test_string_replace(void) {
	typedef struct test {
		char const *str, *pattern, *replacement;
		char const* expect;
		bool ret;
	} test;

#define TEST_COUNT 9
	test const tests[TEST_COUNT] = {
		// Successful replaces:
		{.str = "hello",
		 .pattern = "hell",
		 .replacement = "op",
		 .expect = "opo",
		 .ret = true},
		{.str = "helloh",
		 .pattern = "h",
		 .replacement = "vv",
		 .expect = "vvellovv",
		 .ret = true},
		{.str = "helloh",
		 .pattern = "h",
		 .replacement = "z",
		 .expect = "zelloz",
		 .ret = true},
		{.str = "helloh",
		 .pattern = "ll",
		 .replacement = " verylongtext ",
		 .expect = "he verylongtext oh",
		 .ret = true},
		// Not found:
		{.str = "hello",
		 .pattern = "hey",
		 .replacement = "op",
		 .expect = "hello",
		 .ret = true},
		{.str = "hello",
		 .pattern = "o8",
		 .replacement = "x",
		 .expect = "hello",
		 .ret = true},
		// Pattern too long:
		{.str = "h",
		 .pattern = "hhh",
		 .replacement = "x",
		 .expect = "h",
		 .ret = true},
		// Same string and pattern:
		{.str = "same$",
		 .pattern = "same$",
		 .replacement = "x",
		 .expect = "x",
		 .ret = true},
		// Newlines and tabs:
		{.str = "I'm\t weird \n but should \rwork",
		 .pattern = "should",
		 .replacement = "will",
		 .expect = "I'm\t weird \n but will \rwork",
		 .ret = true},
	};

	for (size_t i = 0; i < TEST_COUNT; i++) {
		arena_checkpoint* cp = arena_get_checkpoint(arena);
		string str, pattern, replacement, expect, out;

		test t = tests[i];
		TEST_ASSERT_TRUE(string_create(arena, &str, t.str));
		TEST_ASSERT_TRUE(string_create(arena, &pattern, t.pattern));
		TEST_ASSERT_TRUE(string_create(arena, &replacement, t.replacement));
		TEST_ASSERT_TRUE(string_create(arena, &expect, t.expect));

		TEST_ASSERT_EQUAL(
			t.ret, string_replace(arena, &out, &str, &pattern, &replacement));
		char msg[256];
		snprintf(msg, 256, "bad strlen for string %s", out.str);
		TEST_ASSERT_EQUAL_MESSAGE(strlen(t.expect), out.len, msg);
		TEST_ASSERT_EQUAL(0, strncmp(t.expect, out.str, out.len));
		arena_restore_checkpoint(arena, cp, true);
	}
#undef TEST_COUNT
}

void test_string_ltrim(void) {
	string s;
	TEST_ASSERT_TRUE(string_create(arena, &s, "abcdef"));

	TEST_ASSERT_TRUE(string_ltrim(&s, 0));
	TEST_ASSERT_EQUAL(6, s.len);

	TEST_ASSERT_TRUE(string_ltrim(&s, 2));
	TEST_ASSERT_EQUAL(4, s.len);
	TEST_ASSERT_EQUAL('c', *s.str);
	TEST_ASSERT_EQUAL('f', s.str[s.len - 1]);

	TEST_ASSERT_FALSE(string_ltrim(&s, 4));
	TEST_ASSERT_FALSE(string_ltrim(&s, 5));
}

void test_string_rtrim(void) {
	string s;
	TEST_ASSERT_TRUE(string_create(arena, &s, "abcdef"));

	TEST_ASSERT_TRUE(string_rtrim(&s, 0));
	TEST_ASSERT_EQUAL(6, s.len);

	TEST_ASSERT_TRUE(string_rtrim(&s, 2));
	TEST_ASSERT_EQUAL(4, s.len);
	TEST_ASSERT_EQUAL('a', *s.str);
	TEST_ASSERT_EQUAL('d', s.str[s.len - 1]);

	TEST_ASSERT_FALSE(string_rtrim(&s, 4));
	TEST_ASSERT_FALSE(string_rtrim(&s, 5));
}

void test_string_trimspace(void) {
	string none;
	string lead;
	string trail;
	string both;
	TEST_ASSERT_TRUE(string_create(arena, &none, "leading and trailing"));
	TEST_ASSERT_TRUE(
		string_create(arena, &lead, "\t  \n	 leading and trailing"));
	TEST_ASSERT_TRUE(
		string_create(arena, &trail, "leading and trailing\t  \r\n "));
	TEST_ASSERT_TRUE(
		string_create(arena, &both, "   \t leading and trailing\t  \r\n "));

	TEST_ASSERT_TRUE(string_trimspace(&none));
	TEST_ASSERT_TRUE(string_trimspace(&lead));
	TEST_ASSERT_TRUE(string_trimspace(&trail));
	TEST_ASSERT_TRUE(string_trimspace(&both));

	size_t len = strlen("leading and trailing");
	TEST_ASSERT_EQUAL(20, len);
	TEST_ASSERT_EQUAL(len, none.len);
	TEST_ASSERT_EQUAL(len, lead.len);
	TEST_ASSERT_EQUAL(len, trail.len);
	TEST_ASSERT_EQUAL(len, both.len);

	char cstr[20];
	strncpy(cstr, none.str, len);
	TEST_ASSERT_EQUAL(0, strncmp(cstr, "leading and trailing", len));
	strncpy(cstr, lead.str, len);
	TEST_ASSERT_EQUAL(0, strncmp(cstr, "leading and trailing", len));
	strncpy(cstr, trail.str, len);
	TEST_ASSERT_EQUAL(0, strncmp(cstr, "leading and trailing", len));
	strncpy(cstr, both.str, len);
	TEST_ASSERT_EQUAL(0, strncmp(cstr, "leading and trailing", len));
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_string_create);
	RUN_TEST(test_string_create_fmt);
	RUN_TEST(test_string_array);
	RUN_TEST(test_string_format_args);
	RUN_TEST(test_string_copy);
	RUN_TEST(test_string_append);
	RUN_TEST(test_string_concat);
	RUN_TEST(test_string_contains_basic);
	RUN_TEST(test_string_contains_same_str);
	RUN_TEST(test_string_contains_last_basic);
	RUN_TEST(test_string_contains_last);
	RUN_TEST(test_string_compare);
	RUN_TEST(test_string_occurences);
	RUN_TEST(test_string_tokenize_regular_trail);
	RUN_TEST(test_string_tokenize_lead);
	RUN_TEST(test_string_tokenize_variable_length);
	RUN_TEST(test_string_tokenize_nomatch);
	RUN_TEST(test_string_tokenize_close_patterns);
	RUN_TEST(test_string_tokenize_close_patterns_many);
	RUN_TEST(test_string_tokenize_only_pattern);
	RUN_TEST(test_string_replace);
	RUN_TEST(test_string_ltrim);
	RUN_TEST(test_string_rtrim);
	RUN_TEST(test_string_trimspace);
	return UNITY_END();
}
