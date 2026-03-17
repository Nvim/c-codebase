add_library(platform SHARED)

target_include_directories(platform PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(platform PUBLIC compile_flags)

if(PLATFORM STREQUAL "LINUX_WAYLAND" OR PLATFORM STREQUAL "LINUX_X11")
	add_subdirectory(linux)
else()
	message(FATAL_ERROR "Invalid platform value: '${PLATFORM}'")
endif()
