#include "queue_internal.h"

#include <allocators/arena.h>
#include <containers/queue.h>
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

void test_queue_create(void) {
	queue_t* queue = queue_create(arena, 32, sizeof(int));
	TEST_ASSERT_NOT_NULL(queue);
	TEST_ASSERT_EQUAL(32, queue_get_remaining(queue));
	TEST_ASSERT_EQUAL(0, queue_get_usage(queue));
	TEST_ASSERT_TRUE(queue_empty(queue));
	TEST_ASSERT_FALSE(queue_full(queue));
}

void test_queue_create_too_much(void) {
	// arena is 1024 => 256 ints - header size
	size_t max_elems =
		(1024 / sizeof(int)) - (sizeof(queue_impl) / sizeof(int));
	TEST_ASSERT_EQUAL(240, max_elems);

	queue_t* queue = queue_create(arena, max_elems, sizeof(int));
	TEST_ASSERT_NOT_NULL(queue);
	TEST_ASSERT_EQUAL(0, arena_get_remaining(arena));

	arena_reset(arena);
	TEST_ASSERT_EQUAL(1024, arena_get_remaining(arena));

	queue = queue_create(arena, max_elems + 1, sizeof(int));
	TEST_ASSERT_NULL(queue);
	TEST_ASSERT_EQUAL(1024, arena_get_remaining(arena));
}

void test_queue_use(void) {
	queue_t* queue = queue_create(arena, 32, sizeof(int));
	TEST_ASSERT_NOT_NULL(queue);

	int elem1 = 42;
	int elem2 = 234419;
	{
		TEST_ASSERT_TRUE(queue_enqueue(queue, &elem1));

		TEST_ASSERT_EQUAL(31, queue_get_remaining(queue));
		TEST_ASSERT_EQUAL(1, queue_get_usage(queue));
		TEST_ASSERT_FALSE(queue_empty(queue));
		TEST_ASSERT_FALSE(queue_full(queue));
	}

	{
		TEST_ASSERT_TRUE(queue_enqueue(queue, &elem2));

		TEST_ASSERT_EQUAL(30, queue_get_remaining(queue));
		TEST_ASSERT_EQUAL(2, queue_get_usage(queue));
	}

	int ret, ret2;
	{
		TEST_ASSERT_TRUE(queue_dequeue(queue, &ret));
		TEST_ASSERT_EQUAL(elem1, ret);
		TEST_ASSERT_EQUAL(31, queue_get_remaining(queue));
		TEST_ASSERT_EQUAL(1, queue_get_usage(queue));
	}
	{
		TEST_ASSERT_TRUE(queue_dequeue(queue, &ret2));
		TEST_ASSERT_EQUAL(elem2, ret2);
		TEST_ASSERT_EQUAL(32, queue_get_remaining(queue));
		TEST_ASSERT_EQUAL(0, queue_get_usage(queue));
	}
	TEST_ASSERT_TRUE(queue_empty(queue));
}

void test_queue_full(void) {
	queue_t* queue = queue_create(arena, 4, sizeof(int));
	TEST_ASSERT_NOT_NULL(queue);

	int elem = 42;
	int ret;
	TEST_ASSERT_TRUE(queue_enqueue(queue, &elem));
	TEST_ASSERT_TRUE(queue_enqueue(queue, &elem));
	TEST_ASSERT_TRUE(queue_enqueue(queue, &elem));
	TEST_ASSERT_TRUE(queue_enqueue(queue, &elem));

	TEST_ASSERT_FALSE(queue_enqueue(queue, &elem));
	TEST_ASSERT_TRUE(queue_full(queue));

	TEST_ASSERT_TRUE(queue_dequeue(queue, &ret));
	TEST_ASSERT_EQUAL(elem, ret);

	TEST_ASSERT_TRUE(queue_enqueue(queue, &elem));
}

void test_queue_empty(void) {
	queue_t* queue = queue_create(arena, 4, sizeof(int));
	TEST_ASSERT_NOT_NULL(queue);

	int elem = 42;
	int ret;
	TEST_ASSERT_TRUE(queue_enqueue(queue, &elem));
	TEST_ASSERT_TRUE(queue_dequeue(queue, &ret));
	TEST_ASSERT_EQUAL(elem, ret);

	int other = 200;
	TEST_ASSERT_TRUE(queue_empty(queue));
	TEST_ASSERT_FALSE(queue_dequeue(queue, &other));
	TEST_ASSERT_EQUAL(200, other);
}

void test_queue_clear(void) {
	queue_t* queue = queue_create(arena, 4, sizeof(int));
	TEST_ASSERT_NOT_NULL(queue);
	for (int i = 0; i < 4; i++) { TEST_ASSERT_TRUE(queue_enqueue(queue, &i)); }
	queue_clear(queue);

	queue_impl* impl = (queue_impl*)queue;
	for (int i = 0; i < 4; i++) {
		TEST_ASSERT_EQUAL(0, *((int*)impl->data + i));
	}

	TEST_ASSERT_EQUAL(0, impl->alloc_count);
	TEST_ASSERT_EQUAL(0, impl->current);
	TEST_ASSERT_EQUAL(0, impl->start);
	TEST_ASSERT_EQUAL(sizeof(int), impl->elem_size);
	TEST_ASSERT_EQUAL(4, impl->buffer_size);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_queue_create);
	RUN_TEST(test_queue_create_too_much);
	RUN_TEST(test_queue_use);
	RUN_TEST(test_queue_full);
	RUN_TEST(test_queue_empty);
	RUN_TEST(test_queue_clear);

	return UNITY_END();
}
