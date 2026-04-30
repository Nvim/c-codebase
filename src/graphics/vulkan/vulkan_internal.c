#include "vulkan_internal.h"

#include <graphics/graphics.h>
#include <string.h>

bool vk_surface_create(vk_surface* out, VkPhysicalDevice device,
					   VkSurfaceKHR surface) {
	if (!surface) { return false; }
	out->surface = surface;
	return vk_surface_get_capabilities(out, device);
}

bool vk_surface_get_capabilities(vk_surface* surface, VkPhysicalDevice device) {
	return check_result(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		device, surface->surface, &surface->capabilities));
}

void vk_surface_destroy(vk_surface* surface, VkInstance instance) {
	vkDestroySurfaceKHR(instance, surface->surface, NULL);
}

static bool _swapchain_refresh_arrays(vk_swapchain* out, VkDevice device) {
	uint32_t old_img_count = out->img_count;
	if (!check_result(vkGetSwapchainImagesKHR(device, out->swapchain,
											  &out->img_count, NULL))) {
		return false;
	}

	// [TODO]: Does image count really change at runtime?
	ASSERT(out->img_count == old_img_count);

	VkImage* p_images = vk_image_array_begin(out->images);
	VkImageView* p_views = vk_image_view_array_begin(out->image_views);
	VkSemaphore* p_sems = vk_sem_array_begin(out->present_semaphores);
	ASSERT(p_images && p_views && p_sems);

	// First clear old semaphores:
	for (uint32_t i = 0; i < out->img_count; ++i) {
		vkDestroySemaphore(device, p_sems[i], NULL);
	}

	if (!check_result(vkGetSwapchainImagesKHR(device, out->swapchain,
											  &out->img_count, p_images))) {
		return false;
	}

	for (uint32_t i = 0; i < out->img_count; ++i) {
		VkImageViewCreateInfo view_ci = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = p_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = out->img_format,
			.subresourceRange = vk_base_subresource_range(),
		};

		VkSemaphoreCreateInfo sem_ci = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

		check_result(vkCreateImageView(device, &view_ci, NULL, &p_views[i]));
		check_result(vkCreateSemaphore(device, &sem_ci, NULL, &p_sems[i]));
	}

	return true;
}

bool vk_swapchain_create(vk_swapchain* out, arena_t* arena, VkDevice device,
						 vk_surface* surface, uint32_t width, uint32_t height,
						 uint32_t min_image_count, VkFormat image_format) {
	VkExtent2D sc_extent = surface->capabilities.currentExtent;
	if (sc_extent.width == 0xFFFFFFFF) {
		sc_extent = (VkExtent2D){.width = width, .height = height};
	}

	out->ci = (VkSwapchainCreateInfoKHR){
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface->surface,
		.minImageCount = min_image_count,
		.imageFormat = image_format,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = sc_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // TODO: from args
		.presentMode = VK_PRESENT_MODE_FIFO_KHR				 // TODO: from args
	};

	out->img_count = 0;
	out->img_index = 0;
	out->img_format = out->ci.imageFormat;

	if (!check_result(
			vkCreateSwapchainKHR(device, &out->ci, NULL, &out->swapchain))) {
		return false;
	}

	if (!check_result(vkGetSwapchainImagesKHR(device, out->swapchain,
											  &out->img_count, NULL))) {
		return false;
	}

	out->images = vk_image_array_create(arena, out->img_count);
	out->image_views = vk_image_view_array_create(arena, out->img_count);
	out->present_semaphores = vk_sem_array_create(arena, out->img_count);
	ASSERT(out->images.raw && out->image_views.raw &&
		   out->present_semaphores.raw);

	return _swapchain_refresh_arrays(out, device);
}

bool vk_swapchain_refresh(vk_swapchain* swapchain, VkDevice device,
						  vk_surface* surface, uint32_t width,
						  uint32_t height) {
	VkExtent2D sc_extent = surface->capabilities.currentExtent;
	if (sc_extent.height == 0 || sc_extent.height == 0) { return false; }
	if (sc_extent.width == 0xFFFFFFFF) {
		sc_extent = (VkExtent2D){.width = width, .height = height};
	}

	{ // Update CI
		swapchain->ci.surface = surface->surface;
		swapchain->ci.imageExtent = sc_extent;
		swapchain->ci.oldSwapchain = swapchain->swapchain;
	}

	{ // Destroy old swapchain
		check_result(vkDeviceWaitIdle(device));
		vk_swapchain_destroy(swapchain, device);
		swapchain->img_index = 0;
	}

	if (!check_result(vkCreateSwapchainKHR(device, &swapchain->ci, NULL,
										   &swapchain->swapchain))) {
		return false;
	}

	return _swapchain_refresh_arrays(swapchain, device);
}

bool vk_swapchain_get_current_image(vk_swapchain* swapchain, VkImage* out) {
	return vk_image_array_get(swapchain->images, out, swapchain->img_index);
}
bool vk_swapchain_get_current_image_view(vk_swapchain* swapchain,
										 VkImageView* out) {
	return vk_image_view_array_get(swapchain->image_views, out,
								   swapchain->img_index);
}
bool vk_swapchain_get_current_semaphore(vk_swapchain* swapchain,
										VkSemaphore* out) {
	return vk_sem_array_get(swapchain->present_semaphores, out,
							swapchain->img_index);
}

vk_sc_image_acquire_result vk_swapchain_acquire_image(vk_swapchain* swapchain,
													  VkDevice device,
													  vk_image_pool image_pool,
													  VkSemaphore semaphore) {
	ASSERT(swapchain->img_index == 0);
	vk_sc_image_acquire_result ret = {0};
	VkResult res =
		vkAcquireNextImageKHR(device, swapchain->swapchain, UINT64_MAX,
							  semaphore, NULL, &swapchain->img_index);

	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		ret.status = ACQUIRE_OUT_OF_DATE;
		return ret;
	}
	if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR) {
		ret.status = ACQUIRE_ERROR;
		return ret;
	}

	LOG_DEBUG("vk_swapchain_acquire_image: acquired image %u",
			  swapchain->img_index);
	gpu_image handle;
	{
		VkImage img;
		VkImageView img_view;
		ASSERT(vk_swapchain_get_current_image(swapchain, &img));
		ASSERT(vk_swapchain_get_current_image_view(swapchain, &img_view));

		vk_image_data data;
		{
			data.image = img;
			data.image_view = img_view;
			data.alloc = NULL;
			data.extent = swapchain->ci.imageExtent;
			data.format = swapchain->img_format;
			data.usage_flags = 0; // Swapchain images don't get any
			data.gpu_usage = GPU_USAGE_NONE;
			data.vk_image_flags = IMAGE_FLAGS_SWAPCHAIN;
		}
		vk_image_pool_handle vk_handle = vk_image_pool_new(image_pool, &data);
		ASSERT(vk_image_pool_is_handle_valid(image_pool, vk_handle));
		handle.handle = vk_handle.raw_handle;
	};

	ret.image = handle;
	ret.status = ACQUIRE_SUCCESS;

	return ret;
}

bool vk_swapchain_present(vk_swapchain* swapchain, VkQueue queue) {
	VkSemaphore wait_semaphore = {0};
	ASSERT(vk_sem_array_get(swapchain->present_semaphores, &wait_semaphore,
							swapchain->img_index));

	LOG_DEBUG("vk_swapchain_present: presenting to image %u",
			  swapchain->img_index);
	VkPresentInfoKHR present_info = {0};
	{
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &wait_semaphore;
		present_info.pImageIndices = &swapchain->img_index;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &swapchain->swapchain;
	}

	// [TODO]: check this result as it can fail when out of date
	vkQueuePresentKHR(queue, &present_info);

	swapchain->img_index = 0;

	return true;
}

void vk_swapchain_destroy(vk_swapchain* swapchain, VkDevice device) {
	ASSERT(swapchain->img_count);
	VkImageView* v = vk_image_view_array_begin(swapchain->image_views);
	VkSemaphore* s = vk_sem_array_begin(swapchain->present_semaphores);
	ASSERT(v);
	if (v) {
		for (uint32_t i = 0; i < swapchain->img_count; ++i) {
			vkDestroyImageView(device, v[i], NULL);
			vkDestroySemaphore(device, s[i], NULL);
		}
	}
	vkDestroySwapchainKHR(device, swapchain->swapchain, NULL);
}

bool vk_render_sync_create(VkDevice device, vk_render_sync* out,
						   vk_cmdbuf_pool cmd_pool,
						   VkCommandPool command_pool) {
	VkSemaphoreCreateInfo sem_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

	// Create fences signalled since there's nothing to wait for on 1st
	// frame
	VkFenceCreateInfo fence_ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	if (!check_result(
			vkCreateSemaphore(device, &sem_ci, NULL, &out->draw_sem))) {
		return false;
	}
	if (!check_result(
			vkCreateFence(device, &fence_ci, NULL, &out->drawn_fence))) {
		return false;
	}

	VkCommandBufferAllocateInfo alloc_info = {0};
	{
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = command_pool;
		alloc_info.level = 0;
	}

	VkCommandBuffer command_buffer;
	if (!check_result(
			vkAllocateCommandBuffers(device, &alloc_info, &command_buffer))) {
		return false;
	}

	vk_cmdbuf_data data = {.command_buffer = command_buffer};
	vk_cmdbuf_pool_handle handle = vk_cmdbuf_pool_new(cmd_pool, &data);
	ASSERT(vk_cmdbuf_pool_is_handle_valid(cmd_pool, handle));
	out->command_buffer = handle;

	return true;
}

void vk_render_sync_destroy(VkDevice device, vk_render_sync* sync) {
	vkDestroySemaphore(device, sync->draw_sem, NULL);
	vkDestroyFence(device, sync->drawn_fence, NULL);
	// Command buffer will be destroyed by the pool
}

bool vk_sync_buffer_create(VkDevice device, vk_sync_buffer* out,
						   vk_cmdbuf_pool cmd_pool, uint32_t frames_in_flight,
						   VkCommandPool command_pool) {
	out->frames_in_flight = frames_in_flight;
	for (uint32_t i = 0; i < out->frames_in_flight; ++i) {
		if (!vk_render_sync_create(device, &out->frames[i], cmd_pool,
								   command_pool)) {
			return false;
		}
	}
	return true;
}

void vk_sync_buffer_destroy(VkDevice device, vk_sync_buffer* out) {
	for (uint32_t i = 0; i < out->frames_in_flight; ++i) {
		vk_render_sync_destroy(device, &out->frames[i]);
	}
}

vk_render_sync* vk_get_current_sync(vk_api_state* state) {
	return &state->sync_buffer
				.frames[state->frame_idx % state->frames_in_flight];
}

bool vk_begin_frame(vk_api_state* state, vk_frame* out,
					window_size window_size) {
	*out = (vk_frame){0};
	if (state->frame_in_progress) {
		LOG_ERR("vulkan_begin_frame: a frame in already in progress");
		return false;
	}

	vk_render_sync* frame_sync = vk_get_current_sync(state);

	static uint64_t const fence_timeout = sec_to_ns(1);
	if (!check_result(vkWaitForFences(state->device, 1,
									  &frame_sync->drawn_fence, false,
									  fence_timeout))) {
		return false;
	};
	if (!check_result(
			vkResetFences(state->device, 1, &frame_sync->drawn_fence))) {
		return false;
	};

	state->frame_in_progress = true;

	gpu_image sc_image;
	{ // Acquire a swapchain image
		LOG_DEBUG("vk_begin_frame: acquiring swapchain image");
		vk_sc_image_acquire_result sc_ret =
			vk_swapchain_acquire_image(&state->swapchain, state->device,
									   state->image_pool, frame_sync->draw_sem);

		if (sc_ret.status == ACQUIRE_ERROR) {
			LOG_ERR("vk_begin_frame: couldn't acquire swapchain image");
			return false;
		}

		if (sc_ret.status == ACQUIRE_OUT_OF_DATE) {
			LOG_INFO("vk_begin_frame: handling out of date swapchain");
			bool ok = vk_swapchain_refresh(&state->swapchain, state->device,
										   &state->surface, window_size.width,
										   window_size.height);
			if (!ok) {
				LOG_ERR("vulkan_begin_frame: couldn't refresh swapchain");
				return false;
			}
		}
		sc_image = sc_ret.image;
	}

	VkSemaphore sc_sem;
	ASSERT(vk_swapchain_get_current_semaphore(&state->swapchain, &sc_sem));

	{
		out->frame_idx = state->frame_idx;
		out->rendertarget = sc_image;
		out->frame_sync = frame_sync;
		out->swapchain_semaphore = sc_sem;
	}

	return true;
}

void vk_image_barrier(VkCommandBuffer cmd_buf, VkImage image,
					  vk_barrier_info from, vk_barrier_info to) {
	VkImageMemoryBarrier2 barrier = {0};
	{
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.image = image;
		barrier.oldLayout = from.layout;
		barrier.newLayout = to.layout;
		barrier.srcStageMask = from.stage;
		barrier.dstStageMask = to.stage;
		barrier.srcAccessMask = from.access;
		barrier.dstAccessMask = to.access;
		barrier.subresourceRange = vk_base_subresource_range();
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	VkDependencyInfo dep_info = {0};
	{
		dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dep_info.imageMemoryBarrierCount = 1;
		dep_info.pImageMemoryBarriers = &barrier;
	}

	vkCmdPipelineBarrier2(cmd_buf, &dep_info);
}

static VkPrimitiveTopology gpu_to_vk_primitive_type(gpu_primitive_type t) {
	switch (t) {
	case GPU_PRIMITIVE_TYPE_TRIANGLELIST:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case GPU_PRIMITIVE_TYPE_TRIANGLESTRIP:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case GPU_PRIMITIVE_TYPE_TRIANGLEFAN:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	case GPU_PRIMITIVE_TYPE_LINELIST: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case GPU_PRIMITIVE_TYPE_LINESTRIP: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case GPU_PRIMITIVE_TYPE_POINTLIST: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	}
}

static VkCompareOp gpu_to_vk_compare_op(gpu_compare_op op) {
	switch (op) {
	case GPU_COMPARE_OP_LESS: return VK_COMPARE_OP_LESS;
	case GPU_COMPARE_OP_LESS_EQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
	case GPU_COMPARE_OP_GREATER: return VK_COMPARE_OP_GREATER;
	case GPU_COMPARE_OP_GREATER_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case GPU_COMPARE_OP_ALWAYS: return VK_COMPARE_OP_ALWAYS;
	case GPU_COMPARE_OP_NEVER: return VK_COMPARE_OP_NEVER;
	}
}

static VkCullModeFlagBits gpu_to_vk_cull_mode(gpu_cull_mode m) {
	switch (m) {
	case GPU_CULL_MODE_NONE: return VK_CULL_MODE_NONE;
	case GPU_CULL_MODE_FRONT: return VK_CULL_MODE_FRONT_BIT;
	case GPU_CULL_MODE_BACK: return VK_CULL_MODE_BACK_BIT;
	}
}

static VkPolygonMode gpu_to_vk_polygon_mode(gpu_polygon_mode m) {
	switch (m) {
	case GPU_POLYGON_FILL: return VK_POLYGON_MODE_FILL;
	case GPU_POLYGON_LINE: return VK_POLYGON_MODE_LINE;
	case GPU_POLYGON_POINT: return VK_POLYGON_MODE_POINT;
	}
}

static VkFrontFace gpu_to_vk_winding(gpu_winding w) {
	switch (w) {
	case GPU_WINDING_CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
	case GPU_WINDING_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
	}
}

bool gpu_to_vk_pipeline_ci(arena_t* arena, gpu_pipeline_ci* ci,
						   VkGraphicsPipelineCreateInfo* out,
						   vk_api_state* state) {

	memset(out, 0, sizeof(*out));
	vk_shader_data* vs =
		vk_shader_pool_get(state->shader_pool, to_vk_shader(ci->vertex_shader));
	vk_shader_data* fs = vk_shader_pool_get(state->shader_pool,
											to_vk_shader(ci->fragment_shader));

	if (!vs || !vs->module || !fs || !fs->module) {
		LOG_ERR("gpu_to_vulkan_pipeline: invalid shader handles");
		return false;
	}

	VkPipelineViewportStateCreateInfo* viewport_ci =
		arena_push_z(arena, VkPipelineViewportStateCreateInfo);
	{
		viewport_ci->sType =
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_ci->viewportCount = 1;
		viewport_ci->scissorCount = 1;
	}

	// [TODO]: Don't hardcode blending
	VkPipelineColorBlendAttachmentState blend_state = {
		.colorWriteMask = 0xF,
	};
	VkPipelineColorBlendStateCreateInfo* blend_ci =
		arena_push_z(arena, VkPipelineColorBlendStateCreateInfo);
	{
		blend_ci->sType =
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend_ci->attachmentCount = 1;
		blend_ci->pAttachments = &blend_state;
	}

	VkPipelineShaderStageCreateInfo* shader_stages =
		arena_push_array_z(arena, VkPipelineShaderStageCreateInfo, 2);
	{
		shader_stages[0].sType =
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_stages[0].module = vs->module;
		shader_stages[0].pName = "main";

		shader_stages[1].sType =
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_stages[1].module = fs->module;
		shader_stages[1].pName = "main";
	}

	// No need for vertex input:
	VkPipelineVertexInputStateCreateInfo* vert_input_ci =
		arena_push_z(arena, VkPipelineVertexInputStateCreateInfo);
	{
		vert_input_ci->sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	}

	VkFormat* formats =
		arena_push_array_z(arena, VkFormat, GPU_MAX_RENDERTARGETS);
	for (uint32_t i = 0; i < ci->color_target_count; ++i) {
		formats[i] = gpu_to_vk_image_format(ci->color_targets[i].format);
	}

	VkPipelineRenderingCreateInfo* _renderInfo =
		arena_push_z(arena, VkPipelineRenderingCreateInfo);
	{
		_renderInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		_renderInfo->colorAttachmentCount = ci->color_target_count;
		_renderInfo->pColorAttachmentFormats = formats;
		_renderInfo->depthAttachmentFormat =
			gpu_to_vk_image_format(ci->depth.depth_target_format);
	};

	VkPipelineRasterizationStateCreateInfo* raster_ci =
		arena_push_z(arena, VkPipelineRasterizationStateCreateInfo);
	{
		raster_ci->sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster_ci->cullMode = gpu_to_vk_cull_mode(ci->rasterizer.cull_mode);
		raster_ci->polygonMode =
			gpu_to_vk_polygon_mode(ci->rasterizer.polygon_mode);
		raster_ci->frontFace = gpu_to_vk_winding(ci->rasterizer.winding_order);
		raster_ci->lineWidth = 1.f;
	}

	VkPipelineInputAssemblyStateCreateInfo* input_asm =
		arena_push_z(arena, VkPipelineInputAssemblyStateCreateInfo);
	{
		input_asm->sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		input_asm->topology =
			gpu_to_vk_primitive_type(ci->rasterizer.primitive_type);
	}

	// [TODO]: Don't hardcode multisampling
	VkPipelineMultisampleStateCreateInfo* multisampling_ci =
		arena_push_z(arena, VkPipelineMultisampleStateCreateInfo);
	{
		multisampling_ci->sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling_ci->rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	};

	// [TODO]: Stencil support
	VkPipelineDepthStencilStateCreateInfo* depth_ci =
		arena_push_z(arena, VkPipelineDepthStencilStateCreateInfo);
	{
		depth_ci->sType =
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_ci->depthTestEnable = ci->depth.depth_test;
		depth_ci->depthWriteEnable = ci->depth.depth_write;
		depth_ci->stencilTestEnable = VK_FALSE;
		depth_ci->depthCompareOp =
			gpu_to_vk_compare_op(ci->depth.depth_compare_op);
	}

	VkDynamicState* dyn_state = arena_push_array_z(arena, VkDynamicState, 2);
	{
		dyn_state[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dyn_state[1] = VK_DYNAMIC_STATE_SCISSOR;
	}

	VkPipelineDynamicStateCreateInfo* dynamic_ci =
		arena_push_z(arena, VkPipelineDynamicStateCreateInfo);
	{
		dynamic_ci->sType =
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_ci->pDynamicStates = dyn_state;
		dynamic_ci->dynamicStateCount = 2;
	};

	{
		out->sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		out->pNext = _renderInfo;
		out->stageCount = 2;
		out->pStages = shader_stages;
		out->pVertexInputState = vert_input_ci;
		out->pInputAssemblyState = input_asm;
		out->pViewportState = viewport_ci;
		out->pRasterizationState = raster_ci;
		out->pMultisampleState = multisampling_ci;
		out->pColorBlendState = blend_ci;
		out->pDepthStencilState = depth_ci;
		out->pDynamicState = dynamic_ci;
	}

	return true;
}

vk_image_pool_handle vk_create_image(VmaAllocator allocator,
									 vk_image_pool image_pool,
									 VkExtent2D extent, VkFormat format,
									 VkImageUsageFlags usage, uint32_t levels,
									 uint32_t queue_family_idx) {

	VkImageCreateInfo img_info = {0};
	{
		img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		img_info.imageType = VK_IMAGE_TYPE_2D;
		img_info.usage = usage;
		img_info.format = format;
		img_info.extent = (VkExtent3D){
			.width = extent.width,
			.height = extent.height,
			.depth = 1,
		};
		img_info.arrayLayers = 1;
		img_info.mipLevels = levels;
		img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		img_info.samples = VK_SAMPLE_COUNT_1_BIT;
		img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		img_info.queueFamilyIndexCount = 1;
		img_info.pQueueFamilyIndices = &queue_family_idx;
	}

	VkImage img;
	VmaAllocationCreateInfo alloc_info = {0};
	{ alloc_info.usage = VMA_MEMORY_USAGE_AUTO; }
	VmaAllocation alloc;

	if (!check_result(vmaCreateImage(allocator, &img_info, &alloc_info, &img,
									 &alloc, NULL))) {
		return VK_IMGPOOL_INVALID;
	}

	VkImageView view = NULL; // TODO

	vk_image_data data = {0};
	{
		data.image = img;
		data.image_view = view;
		data.alloc = alloc;
		data.format = format;
		data.extent = extent;
		data.usage_flags = usage;
		data.gpu_usage = 0; // TODO
	}

	return vk_image_pool_new(image_pool, &data);
}

gpu_image_extent vk_to_gpu_image_extent(VkExtent2D e) {
	return (gpu_image_extent){.width = e.width, .height = e.height};
}

VkExtent2D gpu_to_vk_image_extent(gpu_image_extent e) {
	return (VkExtent2D){.width = e.width, .height = e.height};
}

// gpu_rendertarget vk_to_gpu_rendertarget(gpu_image* t) {
// 	return (gpu_rendertarget){.extent = vk_to_gpu_image_extent(t->extent),
// 							  .format = vk_to_gpu_image_format(t->format),
// 							  .image = t->image};
// }

gpu_frame vk_to_gpu_frame(vk_frame* f) {
	return (gpu_frame){
		.frame_index = f->frame_idx,
		.backbuffer = f->rendertarget,
		.frame_cmd = (gpu_command_buffer){
			.handle = f->frame_sync->command_buffer.raw_handle.handle}};
}

VkImageSubresourceRange vk_base_subresource_range(void) {
	static VkImageSubresourceRange const view_range = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = 1,
		.layerCount = 1,
	};
	return view_range;
}

VkFormat gpu_to_vk_image_format(gpu_image_format f) {
	switch (f) {
	case IMAGE_FORMAT_RGBA_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
	case IMAGE_FORMAT_RGBA_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;
	case IMAGE_FORMAT_RGBA_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;

	case IMAGE_FORMAT_RGB_UNORM: return VK_FORMAT_R8G8B8_UNORM;
	case IMAGE_FORMAT_RGB_SNORM: return VK_FORMAT_R8G8B8_SNORM;
	case IMAGE_FORMAT_RGB_SRGB: return VK_FORMAT_R8G8B8_SRGB;
	case IMAGE_FORMAT_D16: return VK_FORMAT_D16_UNORM;
	}
}

gpu_image_format vk_to_gpu_image_format(VkFormat f) {
	switch (f) {
	case VK_FORMAT_R8G8B8A8_UNORM: return IMAGE_FORMAT_RGBA_UNORM;
	case VK_FORMAT_R8G8B8A8_SNORM: return IMAGE_FORMAT_RGBA_SNORM;
	case VK_FORMAT_R8G8B8A8_SRGB: return IMAGE_FORMAT_RGBA_SRGB;
	case VK_FORMAT_R8G8B8_UNORM: return IMAGE_FORMAT_RGB_UNORM;
	case VK_FORMAT_R8G8B8_SNORM: return IMAGE_FORMAT_RGB_SNORM;
	case VK_FORMAT_R8G8B8_SRGB: return IMAGE_FORMAT_RGB_SRGB;
	default: ASSERT(false); return IMAGE_FORMAT_RGB_UNORM;
	}
}

VkShaderStageFlags gpu_to_vk_shader_stage(gpu_shader_type t) {
	switch (t) {
	case GPU_SHADER_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
	case GPU_SHADER_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
	case GPU_SHADER_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
	}
	return 0;
}
