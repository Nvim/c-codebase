#version 450
#extension GL_EXT_buffer_reference : require

layout (location=0) out vec3 outPos;
layout (location=2) out vec2 outUv;

struct Vertex {
    vec3 position;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants {
    VertexBuffer vertex_buffer;
} push_constants;

const vec2 uvs[6] = vec2[6](
		vec2(0.f, 0.f), vec2(1.f, 1.f), vec2(1.f, 0.f),
		vec2(0.f, 0.f), vec2(0.f, 1.f), vec2(1.f, 1.f)
		);

void main()
{
    Vertex vert = push_constants.vertex_buffer.vertices[gl_VertexIndex];
	outPos = vert.position;
	outUv = uvs[gl_VertexIndex];
	gl_Position = vec4(vert.position, 1.f);
}
