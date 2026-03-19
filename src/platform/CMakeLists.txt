add_library(platform SHARED)

target_include_directories(platform PUBLIC ${PROJECT_SOURCE_DIR}/include)
# Needed for graphics:
target_include_directories(platform PUBLIC ${PROJECT_SOURCE_DIR}/src/)
target_link_libraries(platform PUBLIC compile_flags core)

if(PLATFORM STREQUAL "LINUX_WAYLAND" OR PLATFORM STREQUAL "LINUX_X11")
	add_subdirectory(linux)
else()
	message(FATAL_ERROR "Invalid platform value: '${PLATFORM}'")
endif()
