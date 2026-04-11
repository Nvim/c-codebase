- implement pool alloc/getter funcs
- implement begin and end rendering with hardcoded attachment and barriers (assume image is always swapchain)
- debug :)

- vk_barrier utils
- vk_pipeline_base_barrier ? (saves like 3 lines idk)
- don't hardcode the attachment -> use the image intented usage to place barriers when needed

- docs (frame cmd_buf shouldn't be destroyed)
- quake2 like dll at runtime
