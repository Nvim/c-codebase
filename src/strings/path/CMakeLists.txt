target_sources(strings PRIVATE path.c)

# ---- Tests ----
add_executable(path_test path_test.c)
target_link_libraries(path_test PRIVATE compile_flags strings containers unity)

add_test(NAME path_test COMMAND path_test)
