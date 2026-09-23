struct DrawItem {
	float4 color;
	float2 position;
	float scale;
};

struct UniformData {
	float4x4 mat_clip_from_world; // view*proj
	DrawItem* draw_items;
	float time;
	float scale;
};

float4x4 translatePosition(float2 pos) {
	return float4x4(
		float4(1.f, 0.f, 0.f, pos.x),
		float4(0.f, 1.f, 0.f, pos.y),
		float4(0.f, 0.f, 1.f, 0.f),
		float4(0.f, 0.f, 0.f, 1.f)
	);
}

float4x4 scaleMatrix(float scale) {
	return float4x4(
		float4(scale, 0.f, 0.f, 0.f),
		float4(0.f, scale, 0.f, 0.f),
		float4(0.f, 0.f, 1.f, 0.f),
		float4(0.f, 0.f, 0.f, 1.f)
	);
}

struct Vertex_Out {
	float4 pos : SV_Position;
	float4 color : SV_Target1;
};

// Push constants
struct ShaderInput {
	uint64_t* reserved;
	UniformData* uniform;
}

static const float2 bot_left = float2(-1.f, -1.f);
static const float2 bot_right = float2(1.f, -1.f);
static const float2 top_right = float2(1.f, 1.f);
static const float2 top_left = float2(-1.f, 1.f);

static const float2 Quad[4] = float2[4](
		bot_left, bot_right, top_right, top_left
    );



[[shader("vertex")]]
Vertex_Out vertMain(uint vertexID : SV_VertexID, uint instanceId: SV_InstanceID, uniform ShaderInput constants) {
	Vertex_Out ret;

	DrawItem item = constants.uniform.draw_items[instanceId];
	float4 baseCoord = float4(Quad[vertexID], 1.f);
	float4x4 model = mul(scaleMatrix(constants.uniform.scale), translatePosition(item.position));
	float4x4 mvp = mul(constants.uniform.mat_clip_from_world, model);
	ret.pos = mul(mvp, baseCoord);
	ret.color = item.color;
	return ret;
}

struct Fragment_Out {
	float4 color : SV_Target0;
};

[[shader("pixel")]]
Fragment_Out fragMain(Vertex_Out input, uniform ShaderInput constants) {
	Fragment_Out ret;

	ret.color = input.color;

	return ret;
}
