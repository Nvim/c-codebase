target_sources(allocators PRIVATE arena.c)

# ---- Tests ----
add_executable(arena_test arena_test.c)
target_link_libraries(arena_test PRIVATE compile_flags allocators unity)

add_test(NAME arena_test COMMAND arena_test)
