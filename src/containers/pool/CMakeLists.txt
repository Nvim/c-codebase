target_sources(containers PRIVATE pool.c)

# ---- Tests ----
add_executable(pool_test pool_test.c)
target_link_libraries(pool_test PRIVATE compile_flags containers unity)

add_test(NAME pool_test COMMAND pool_test)
