add_library(graphics SHARED)

# Always link with Vulkan SDK, I don't support anything else for now
find_package(Vulkan REQUIRED)

add_library(vma)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
)
set(VMA_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

target_link_libraries(vma PRIVATE GPUOpen::VulkanMemoryAllocator)
target_compile_options(vma PRIVATE -fPIC)
target_sources(vma PRIVATE vma.cpp)

target_include_directories(graphics PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(graphics PUBLIC compile_flags core allocators containers strings Vulkan::Vulkan)
target_link_libraries(graphics PRIVATE vma)

target_sources(graphics PUBLIC vulkan.c vulkan_internal.c)
