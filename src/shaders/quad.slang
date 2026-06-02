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

float2x2 rotationMatrix(float theta) {
	return float2x2(
    cos(theta), -sin(theta),
    sin(theta),  cos(theta));
}

[[shader("vertex")]]
VSOutput vertMain(uint vertexID : SV_VertexID, uniform VSInput constants) {
	VSOutput ret;

	const float speed = .5f;
	Vertex vert = constants.uniform[0].vertex_buffer[vertexID];

	ret.pos = float4(mul(vert.position, .75f), 1.f);
	ret.uv = vert.uv;

	float2x2 mat = rotationMatrix(constants.uniform.time*speed);
	ret.pos.xy = mul(mat, ret.pos.xy);

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

	return ret;
}
