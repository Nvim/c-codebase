target_sources(containers PRIVATE hashtable.c)

# ---- Tests ----
add_executable(hashtable_test hashtable_test.c)
target_link_libraries(hashtable_test PRIVATE compile_flags containers strings unity)

add_test(NAME hashtable_test COMMAND hashtable_test)
