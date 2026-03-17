add_executable(main main.c)
target_link_libraries(main PUBLIC
	compile_flags
	platform
	core
)

