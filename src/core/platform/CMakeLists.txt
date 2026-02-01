target_sources(core PRIVATE platform_memory.c)

# ---- Tests ----
add_executable(platform_memory_test platform_memory_test.c)
target_link_libraries(platform_memory_test PRIVATE compile_flags core unity)
add_test( NAME platform_memory_test COMMAND platform_memory_test)
