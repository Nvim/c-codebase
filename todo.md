# containers

[ ] pool iterate

# gpu

[ ] quake2 like dll at runtime
[x] implement pool alloc/getter funcs
[x] implement begin and end rendering with hardcoded attachment and barriers (assume image is always swapchain)
[x] debug :)
[x] vk_barrier utils
[ ] vk_pipeline_base_barrier ? (saves like 3 lines idk)
[x] don't hardcode the attachment -> use the image intented usage to place barriers when needed
[x] docs (frame cmd_buf shouldn't be destroyed)
[ ] make opaque handle for basic resources (e.g sampler,fence) better (union on `VkXXX`+debug name?)
[ ] debug name strings for every resource
[ ] pool cleanup (needs iterate API)

## pipeline

[x] implement gpu_ci builder
[x] implement gpu_ci -> VkPipelineCreateInfo & friends
[x] vk_pipeline_pool & data struct (store ci along pipeline?)
[x] improve vulkan_cmd_begin_rendering to actually use every field from render_state
[x] depth
