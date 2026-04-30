#include <graphics/graphics.h>
#include <string.h>

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
