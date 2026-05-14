# containers

[ ] pool iterate

# gpu

[ ] quake2 like dll at runtime
[x] implement pool alloc/getter funcs
[x] implement begin and end rendering with hardcoded attachment and barriers (assume image is always swapchain)
[x] debug :)
[x] vk_barrier utils
[x] don't hardcode the attachment -> use the image intented usage to place barriers when needed
[x] docs (frame cmd_buf shouldn't be destroyed)
[ ] make opaque handle for basic resources (e.g sampler,fence) better (union on `VkXXX`+debug name?)
[ ] debug name strings for every resource
[ ] pool cleanup (needs iterate API)

## threading

[ ] queue selection: try to get separate graphics & transfer if possible
[ ] command buffer submit: add preferred queue type hint to API
[ ] command buffer submit: give & accept semaphores
[ ] frame sync: add a dedicated transfer cmdbuffer to frame? (maybe in renderer-level)

## pipeline

[x] implement gpu_ci builder
[x] implement gpu_ci -> VkPipelineCreateInfo & friends
[x] vk_pipeline_pool & data struct (store ci along pipeline?)
[x] improve vulkan_cmd_begin_rendering to actually use every field from render_state
[x] depth

## buffers

**usages**:
- INDEX
- STORAGE
- STAGING_UP
- INDIRECT (later)
- STAGING_DOWN (later)

**memory type (internal)**:
- GPU_ONLY -> updates via staging
- MAPPED -> bool flag to put on gpu if possible, else cpu (vma handles it)

- INDEX must be GPU_ONLY
- STAGING_* must be MAPPED
- let user decide STORAGE and INDIRECT


**api**:
- gpu_buffer_ci type
- span type
- buffer_upload_info type (span+offset)
- create_buffer
- destroy_buffer
- get_mapped -> void* (MAPPED only)
- cmd_upload_to_buffer(buf, info)

[ ] do it
[ ] cleanup image upload flow to use user's transfer buffer

## views

[ ] give image_view a *view_type* inspired by sokol
[ ] do i need buffer views?
