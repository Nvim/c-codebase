target_sources(strings PRIVATE string.c)

# ---- Tests ----
add_executable(string_test string_test.c)
target_link_libraries(string_test PRIVATE compile_flags strings core containers unity)

add_test(NAME string_test COMMAND string_test)
