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

struct VSInput {
	uint64_t* reserved_buffer; // unused
	Vertex* vertex_buffer;
};

[[shader("vertex")]]
VSOutput vertMain(uint vertexID : SV_VertexID, uniform VSInput constants) {
	VSOutput ret;

	Vertex vert = constants.vertex_buffer[vertexID];

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
FSOutput fragMain(VSOutput input) {
	FSOutput ret;

	// TODO: get image index as parameter in frame user data
	ret.color = fs_input.Texture[4].Sample(input.uv);

	return ret;
}
