cmake_minimum_required(VERSION 3.35)
project(containers LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/thirdparty")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Set build mode to debug" FORCE)
endif()

enable_testing()

set(PLATFORM "LINUX_WAYLAND" CACHE STRING "Target platform (LINUX_WAYLAND or LINUX_X11)")
if(PLATFORM STREQUAL "LINUX_WAYLAND")
    add_compile_definitions(PLATFORM_LINUX=1)
	add_compile_definitions(WAYLAND=1)
elseif(PLATFORM STREQUAL "LINUX_X11")
    add_compile_definitions(PLATFORM_LINUX=1)
	add_compile_definitions(X11=1)
else()
	message(FATAL_ERROR "Invalid platform value: '${PLATFORM}'")
endif()

# Always on flags & defines:
add_library(compile_flags INTERFACE)
target_compile_options(compile_flags
    INTERFACE
        -Wall
        -Wextra
        -Werror
        -Wvla
        -Wno-gnu-zero-variadic-macro-arguments
        -pedantic
        -fPIC
)

target_compile_definitions(compile_flags
    INTERFACE
        _DEFAULT_SOURCE
)

# Build type specific:
target_compile_options(compile_flags
    INTERFACE
        $<$<CONFIG:Debug>:-O0 -g3 -fno-inline -fno-omit-frame-pointer -fvisibility=default>
        $<$<NOT:$<CONFIG:Debug>>:-fvisibility=hidden>
)

target_compile_definitions(compile_flags
    INTERFACE
        $<$<CONFIG:Debug>:_DEBUG>
)

target_include_directories(compile_flags
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

if(NOT MSVC)
    target_link_options(compile_flags
        INTERFACE
            -Wl,--no-undefined
    )
endif()

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# Make GLFW available (downloads and integrates it)
FetchContent_MakeAvailable(glfw)

add_subdirectory(tests)
add_subdirectory(src/platform)
add_subdirectory(src/core)
add_subdirectory(src/allocators)
add_subdirectory(src/containers)
add_subdirectory(src/strings)
add_subdirectory(src/filesystem)
add_subdirectory(src/graphics)

add_subdirectory(runtime)
