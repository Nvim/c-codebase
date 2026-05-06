# gpu

[x] quake2 like dll at runtime
[x] implement pool alloc/getter funcs
[x] implement begin and end rendering with hardcoded attachment and barriers (assume image is always swapchain)
[x] debug :)
[x] vk_barrier utils
[ ] vk_pipeline_base_barrier ? (saves like 3 lines idk)
[x] don't hardcode the attachment -> use the image intented usage to place barriers when needed

[x] docs (frame cmd_buf shouldn't be destroyed)

## pipeline

[ ] implement gpu_ci builder
[ ] implement gpu_ci -> VkPipelineCreateInfo & friends
[ ] vk_pipeline_pool & data struct (store ci along pipeline?)
[ ] improve vulkan_cmd_begin_rendering to actually use every field from render_state
[ ] depth
