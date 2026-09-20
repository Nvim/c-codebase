struct Vertex_Out {
	float4 pos : SV_Position;
};

struct Vertex {
	float3 position;
	float _pad1;
	float3 color;
	float _pad2;
};

struct UniformData {
	float4x4 mat_clip_from_world; // view*proj
	float4x4 mat_world_from_model; // model
	float time;
};

// Push constants
struct ShaderInput {
	uint64_t* reserved;
	UniformData* uniform;
}

[[shader("vertex")]]
Vertex_Out vertMain(uint vertexID : SV_VertexID, uniform ShaderInput constants) {
	Vertex_Out ret;

	uint b = 1 << (vertexID % 6);
	float4 baseCoord = float4(
		(0x1C & b) != 0 ? .8f : -.8f,
		(0xE & b) != 0 ? .8f : -.8f,
		0.f,
		1.f,
	);

	float4x4 mvp = mul(constants.uniform.mat_clip_from_world, constants.uniform.mat_world_from_model);
	ret.pos = mul(mvp, baseCoord);
	return ret;
}

struct Fragment_Out {
	float4 color : SV_Target0;
};

[[shader("pixel")]]
Fragment_Out fragMain(Vertex_Out input, uniform ShaderInput constants) {
	Fragment_Out ret;

	ret.color = float4(.9f, .1f, .85f, 1.f);

	return ret;
}
