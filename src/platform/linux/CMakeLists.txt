target_sources(platform PRIVATE
	memory.c
	filesystem.c
	window.c
)
target_link_libraries(platform PRIVATE compile_flags glfw)

# ---- Tests ----
add_executable(memory_test memory_test.c)
target_link_libraries(memory_test PRIVATE compile_flags glfw platform unity)

add_test(NAME memory_test COMMAND memory_test)
