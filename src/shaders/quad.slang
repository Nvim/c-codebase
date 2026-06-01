struct VSOutput {
	float4 pos : SV_Position;
	float2 uv : SV_Target1;
};

struct Vertex {
    float3 position;
	float _pad1;
	float2 uv;
	float2 _pad2;
};

struct UniformData {
	Vertex* vertex_buffer;
	float time;
	uint image_index;
};

// Push constant range is the same for both vertex & fragment stages for now
struct VSInput {
	uint64_t* reserved_buffer; // unused
	UniformData* uniform;
};

[[shader("vertex")]]
VSOutput vertMain(uint vertexID : SV_VertexID, uniform VSInput constants) {
	VSOutput ret;

	Vertex vert = constants.uniform[0].vertex_buffer[vertexID];

	ret.pos = float4(vert.position, 1.f);
	ret.uv = vert.uv;

	return ret;
}

// == FRAGMENT ==
struct FSOutput {
	float4 color : SV_Target0;
};

struct FSInput {
	Sampler2D[] Texture;
};

ParameterBlock<FSInput> fs_input;

[[shader("pixel")]]
FSOutput fragMain(VSOutput input, uniform VSInput constants) {
	FSOutput ret;

	uint tex_idx = constants.uniform[0].image_index;
	ret.color = fs_input.Texture[tex_idx].Sample(input.uv);
	float fact = sin(constants.uniform[0].time*0.05);
	ret.color = mul(ret.color, fact);
	return ret;
}
