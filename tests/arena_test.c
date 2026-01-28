#include "unity.h"
#include "arena.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_arena_create(void) {
	arena_t* arena = arena_create(28);
	TEST_ASSERT_NOT_NULL(arena);
	arena_destroy(arena);
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_create);
    return UNITY_END();
}
