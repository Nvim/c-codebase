#version 450
#extension GL_EXT_buffer_reference : require

layout (location=0) out vec3 outPos;
layout (location=2) out vec2 outUv;

struct Vertex {
    vec3 position;
	float _pad1;
	vec2 uv;
	vec2 _pad2;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer ReservedBuffer {
    uint idontneedyoufornow[];
};

layout(push_constant) uniform constants {
	ReservedBuffer reserved_buffer;
    VertexBuffer vertex_buffer;
} push_constants;

void main()
{
    Vertex vert = push_constants.vertex_buffer.vertices[gl_VertexIndex];
	outPos = vert.position;
	outUv = vert.uv;
	gl_Position = vec4(vert.position, 1.f);
}
