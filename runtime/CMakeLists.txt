add_subdirectory(game)

add_executable(platform_demo platform_demo.c)
target_link_libraries(platform_demo PUBLIC
	compile_flags
	platform
	core
)

add_executable(gpu_demo gpu_demo.c)
target_link_libraries(gpu_demo PRIVATE
	compile_flags
	platform
	core
	graphics
	loaders
)

add_executable(gpu_demo_nofence gpu_demo_nofence.c)
target_link_libraries(gpu_demo_nofence PRIVATE
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

