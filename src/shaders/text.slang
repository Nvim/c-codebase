/* == Vertex == */
struct VSOutput {
	float4 pos : SV_Position;
	float2 uv : SV_Target1;
};

struct Vertex {
	float2 pos;
	float2 uv;
}

struct UniformData {
	Vertex* vertex_buffer;
	uint image_index;
	uint sampler_index;
};

struct VSInput {
	uint64_t* reserved_buffer; // unused
	UniformData* uniform;
};

[[shader("vertex")]]
VSOutput vertMain(uint vertexID : SV_VertexID, uniform VSInput constants) {
	VSOutput ret;

	Vertex vert = constants.uniform[0].vertex_buffer[vertexID];

	ret.pos = float4(vert.pos, 1.f);
	ret.uv = vert.uv;

	return ret;
}

/* == FRAGMENT == */
struct FSOutput {
	float4 color : SV_Target0;
};

// [NOTE]: need to find a way to make bindings more portable, not a concern for now
[[vk::binding(0, 0)]]
Texture2D[] textures;

[[vk::binding(1, 0)]]
RWTexture2D<float4>[] images;

[[vk::binding(2, 0)]]
SamplerState[] samplers;

[[shader("pixel")]]
FSOutput fragMain(VSOutput input, uniform VSInput constants) {
	FSOutput ret;

	uint tex_idx = constants.uniform[0].image_index;
	uint sampler_idx = constants.uniform[0].sampler_index;

	Texture2D tex = textures[tex_idx];
	SamplerState sampler = samplers[sampler_idx];

	// Texture only has red channel
	float color = tex.Sample(sampler, input.uv).x;

	// [TODO]: real blending
	if (color == 0.f) {
		discard;
	}

	ret.color = float4(float3(color), 1.f);
	return ret;
}
