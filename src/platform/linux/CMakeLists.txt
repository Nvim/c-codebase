target_sources(platform PRIVATE memory.c filesystem.c)

# ---- Tests ----
add_executable(memory_test memory_test.c)
target_link_libraries(memory_test PRIVATE compile_flags platform unity)

add_test(NAME memory_test COMMAND memory_test)
