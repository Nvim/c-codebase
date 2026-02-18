target_sources(containers PRIVATE queue.c)

# ---- Tests ----
add_executable(queue_test queue_test.c)
target_link_libraries(queue_test PRIVATE compile_flags containers unity)

add_test(NAME queue_test COMMAND queue_test)
