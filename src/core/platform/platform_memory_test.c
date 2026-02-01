#include <core/platform.h>

#include "unity.h"

#include <stdint.h>
#include <string.h>

/* ============================================================
   Helpers
   ============================================================ */

static size_t page_size;

void setUp(void)
{
    page_size = platform_memory_page_size();
    TEST_ASSERT_TRUE(page_size > 0);
}

void tearDown(void)
{
}

/* ============================================================
   Alignment helpers
   ============================================================ */

void test_page_begin(void)
{
    void *addr = (void *)(page_size * 3 + 123);
    void *begin = platform_memory_page_begin(addr);

    TEST_ASSERT_EQUAL_PTR((void *)(page_size * 3), begin);
}

void test_next_page_always_advances(void)
{
    TEST_ASSERT_EQUAL_PTR((void *)page_size,
                           platform_memory_next_page((void *)0));

    TEST_ASSERT_EQUAL_PTR((void *)page_size,
                           platform_memory_next_page((void *)1));

    TEST_ASSERT_EQUAL_PTR((void *)(page_size * 2),
                           platform_memory_next_page((void *)page_size));
}

/* ============================================================
   Reserve / Commit / Decommit
   ============================================================ */

void test_reserve_returns_page_aligned_address(void)
{
    void *mem = platform_memory_reserve(page_size * 2);
    TEST_ASSERT_NOT_NULL(mem);

    TEST_ASSERT_EQUAL_PTR(mem, platform_memory_page_begin(mem));
}

void test_commit_rounds_size_up(void)
{
    void *mem = platform_memory_reserve(page_size * 2);
    TEST_ASSERT_NOT_NULL(mem);

    /* Commit less than two pages */
    TEST_ASSERT_TRUE(platform_memory_commit(mem, page_size + 1));

    /* Touch both pages – should not segfault */
    memset(mem, 0xAA, page_size * 2);
}

void test_decommit_drops_access(void)
{
    void *mem = platform_memory_reserve(page_size * 2);
    TEST_ASSERT_NOT_NULL(mem);

    TEST_ASSERT_TRUE(platform_memory_commit(mem, page_size * 2));

    memset(mem, 0xBB, page_size * 2);

    TEST_ASSERT_TRUE(platform_memory_decommit(mem, page_size * 2));

    /* We can't legally touch it now — behavior is SIGSEGV.
       So we only verify recommit works. */
    TEST_ASSERT_TRUE(platform_memory_commit(mem, page_size * 2));

    memset(mem, 0xCC, page_size * 2);
}

void test_commit_rejects_unaligned_address(void)
{
    void *mem = platform_memory_reserve(page_size * 2);
    TEST_ASSERT_NOT_NULL(mem);

    void *unaligned = (void *)((uintptr_t)mem + 1);

    TEST_ASSERT_FALSE(platform_memory_commit(unaligned, page_size));
}

void test_commit_rejects_zero_size(void)
{
    void *mem = platform_memory_reserve(page_size);
    TEST_ASSERT_NOT_NULL(mem);

    TEST_ASSERT_FALSE(platform_memory_commit(mem, 0));
}

/* ============================================================
   malloc wrappers
   ============================================================ */

void test_alloc_and_free(void)
{
    void *mem = platform_memory_alloc(128);
    TEST_ASSERT_NOT_NULL(mem);

    memset(mem, 0x11, 128);

    platform_memory_free(mem);
}

/* ------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_page_begin);
    RUN_TEST(test_next_page_always_advances);

    RUN_TEST(test_reserve_returns_page_aligned_address);
    RUN_TEST(test_commit_rounds_size_up);
    RUN_TEST(test_decommit_drops_access);
    RUN_TEST(test_commit_rejects_unaligned_address);
    RUN_TEST(test_commit_rejects_zero_size);

    RUN_TEST(test_alloc_and_free);

    return UNITY_END();
}
