add_library(filesystem SHARED filesystem.c)
target_include_directories(filesystem PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(filesystem PUBLIC compile_flags allocators containers strings)

target_sources(filesystem PRIVATE filesystem.c)

# ---- Tests ----
add_executable(filesystem_test filesystem_test.c)
target_link_libraries(filesystem_test PRIVATE compile_flags filesystem unity)
add_test( NAME filesystem_test COMMAND filesystem_test)

set_tests_properties(filesystem_test
    PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
