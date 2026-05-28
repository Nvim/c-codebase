#include <graphics/graphics.h>
#include <string.h>

/* ========================================================================== */
void gpu_pipeline_builder_create(gpu_pipeline_builder* out) {
	ASSERT(out);
	gpu_pipeline_ci* ci = &out->ci;

	// [NOTE]: 0 is a sane default for everything except depth format
	memset(ci, 0, sizeof(*ci));

	out->ci.depth.depth_target_format = IMAGE_FORMAT_D16;
}

void set_vertex_shader(gpu_pipeline_builder* builder, gpu_shader vs) {
	builder->ci.vertex_shader = vs;
}

void set_fragment_shader(gpu_pipeline_builder* builder, gpu_shader fs) {
	builder->ci.fragment_shader = fs;
}

void add_color_target(gpu_pipeline_builder* builder, gpu_image_format format,
					  bool blend) {
	builder->ci.color_targets[builder->ci.color_target_count++] =
		(gpu_color_target_info){
			.format = format,
			.blend = blend,
		};
}

void enable_depth_write(gpu_pipeline_builder* builder) {
	builder->ci.depth.depth_write = true;
}

void enable_depth_test(gpu_pipeline_builder* builder) {
	builder->ci.depth.depth_test = true;
}

void set_depth_format(gpu_pipeline_builder* builder, gpu_image_format format) {
	builder->ci.depth.depth_target_format = format;
}

void set_depth_compare_op(gpu_pipeline_builder* builder, gpu_compare_op op) {
	builder->ci.depth.depth_compare_op = op;
}

void set_primitive_type(gpu_pipeline_builder* builder,
						gpu_primitive_type type) {
	builder->ci.rasterizer.primitive_type = type;
}

void set_cull_mode(gpu_pipeline_builder* builder, gpu_cull_mode mode) {
	builder->ci.rasterizer.cull_mode = mode;
}

void set_polygon_mode(gpu_pipeline_builder* builder, gpu_polygon_mode mode) {
	builder->ci.rasterizer.polygon_mode = mode;
}

void set_winding_order(gpu_pipeline_builder* builder, gpu_winding winding) {
	builder->ci.rasterizer.winding_order = winding;
}
/* ========================================================================== */

/* ========================================================================== */

static bool is_depth_usage(gpu_image_usage u) {
	return u & GPU_IMAGE_USAGE_DEPTH_TARGET;
}

// Return false if flags has another set
static bool has_another_usage(gpu_image_usage flags, gpu_image_usage expected) {
	return flags & ~expected;
}

static bool is_depth_format(gpu_image_format f) {
	return (f == IMAGE_FORMAT_D16);
}

static bool is_array_type(gpu_image_type t) {
	return (t == GPU_IMAGE_TYPE_2D_ARRAY || t == GPU_IMAGE_TYPE_CUBE_ARRAY);
}

bool gpu_validate_image_ci(gpu_image_ci* ci) {
	if (!ci) { return false; }

	if (is_depth_usage(ci->usage)) {
		if (has_another_usage(ci->usage, GPU_IMAGE_USAGE_DEPTH_TARGET)) {
			LOG_ERR("gpu_validate_image_ci: depth image has invalid usages");
			return false;
		}

		if (ci->type != GPU_IMAGE_TYPE_2D) {
			LOG_ERR("gpu_validate_image_ci: depth images must be 2D");
			return false;
		}

		if (!is_depth_format(ci->format)) {
			LOG_ERR("gpu_validate_image_ci: invalid format for depth image");
			return false;
		}
	}

	if (!is_array_type(ci->type) && ci->array_layers != 1) {
		LOG_ERR("gpu_validate_image_ci: invalid array layers count");
		return false;
	}

	if (ci->type == GPU_IMAGE_TYPE_CUBE_ARRAY) {
		if (ci->array_layers % 6 != 0) {
			LOG_ERR("gpu_validate_image_ci: invalid array layers count");
			return false;
		}
	}

	if (ci->type == GPU_IMAGE_TYPE_CUBE) {
		if (ci->array_layers != 6) {
			LOG_ERR("gpu_validate_image_ci: invalid array layers count");
			return false;
		}
	}

	return true;
}

bool is_buffer_staging(gpu_buffer_usage usage) {
	return (usage == GPU_BUFFER_USAGE_STAGING_DOWN ||
			usage == GPU_BUFFER_USAGE_STAGING_UP);
}

bool gpu_validate_buffer_ci(gpu_buffer_ci* ci) {
	if (!ci) {
		LOG_ERR("gpu_validate_buffer_ci: invalid parameter");
		return false;
	}

	if (ci->usage == GPU_BUFFER_USAGE_STAGING_DOWN) {
		LOG_ERR("gpu_validate_buffer_ci: unsupported usage");
		return false;
	}

	if (ci->usage == GPU_BUFFER_USAGE_INDEX &&
		ci->memory_preference == GPU_BUFFER_MEMORY_MAPPED) {
		LOG_WARN("gpu_validate_buffer_ci: ignoring memory preference for index "
				 "buffer");
		ci->memory_preference = GPU_BUFFER_MEMORY_GPU;
	}

	if (is_buffer_staging(ci->usage) &&
		ci->memory_preference == GPU_BUFFER_MEMORY_GPU) {
		LOG_WARN("gpu_validate_buffer_ci: ignoring memory preference for "
				 "staging buffer");
		ci->memory_preference = GPU_BUFFER_MEMORY_MAPPED;
	}
	return true;
}
