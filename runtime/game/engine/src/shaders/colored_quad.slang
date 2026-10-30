struct DrawItem {
	float4 color;
	float2 position;
	float2 scale;
};

struct Camera {
	float4x4 view;
	float4x4 proj;
};

struct UniformData {
	DrawItem* draw_items;
	Camera* cameras;
	uint32_t camera_index;
	float time;
};

float4x4 translatePosition(float2 pos) {
	return float4x4(
		float4(1.f, 0.f, 0.f, pos.x),
		float4(0.f, 1.f, 0.f, pos.y),
		float4(0.f, 0.f, 1.f, 0.f),
		float4(0.f, 0.f, 0.f, 1.f)
	);
}

float4x4 scaleMatrix(float2 scale) {
	return float4x4(
		float4(scale.x, 0.f, 0.f, 0.f),
		float4(0.f, scale.y, 0.f, 0.f),
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

static const float2 bot_left = float2(-.5f, -.5f);
static const float2 bot_right = float2(.5f, -.5f);
static const float2 top_right = float2(.5f, .5f);
static const float2 top_left = float2(-.5f, .5f);

static const float2 Quad[4] = float2[4](
		bot_left, bot_right, top_right, top_left
    );



[[shader("vertex")]]
Vertex_Out vertMain(uint vertexID : SV_VertexID, uint instanceId: SV_InstanceID, uniform ShaderInput constants) {
	Vertex_Out ret;

	DrawItem item = constants.uniform.draw_items[instanceId];
	Camera camera = constants.uniform.cameras[constants.uniform.camera_index];
	float4 baseCoord = float4(Quad[vertexID], float2(1.f));

	float4x4 model = mul(translatePosition(item.position), scaleMatrix(item.scale));
	float4x4 viewproj = mul(camera.proj, camera.view);
	float4x4 mvp = mul(viewproj, model);

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
