add_executable(main main.c)
target_link_libraries(main PUBLIC
	compile_flags
	platform
	core
)

add_executable(vk vk.c)
target_link_libraries(vk PRIVATE
	compile_flags
	platform
	core
	graphics
	loaders
)

add_executable(json_format json_format.c)
target_link_libraries(json_format PRIVATE
	compile_flags
	platform
	core
	loaders
)

