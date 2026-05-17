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

target_link_libraries(graphics PUBLIC Vulkan::Vulkan)
target_link_libraries(graphics PRIVATE vma)

target_sources(graphics PRIVATE vulkan.c vulkan_internal.c)
