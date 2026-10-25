# C codebase

Not much to see here. Cool things on the way.

### Structure

- `include`: public headers for libraries
- `src/`: sources and tests for public libraries
- `thirdparty/`: vendored libs
- `runtime/`: actual programs that use the public libraries
- `runtime/game`: wip game with it's own engine and dedicated libraries

### external libraries

- [GLFW](https://github.com/glfw/glfw) for creating a window and input handling
- [Unity](https://github.com/ThrowTheSwitch/Unity) for unit tests
- [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
  for Vulkan memory allocation

All of these are included as submodules.

### build

Build everything with CMake, C11 compiler needed. Not tested on Windows. You'll
also need the Vulkan SDK.

Use `CMakePresets.json` for easy recipes:

```bash
mkdir -p build
cmake --preset release-gcc
cmake --build --preset release-gcc
# executables in build/release-gcc/runtime
```

