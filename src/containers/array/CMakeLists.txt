target_sources(containers PRIVATE array.c)

# ---- Tests ----
add_executable(array_test array_test.c)
target_link_libraries(array_test PRIVATE compile_flags containers unity)

add_test(NAME array_test COMMAND array_test)
