#version 460

layout (location=0) out vec3 outPos;
layout (location=1) out vec3 outColor;
layout (location=2) out vec2 outUv;

struct Vertex {
    vec3 position;
	vec3 color;
};

const vec3 BLUE = vec3(0.f, 0.f, 1.f);
const vec3 GREEN = vec3(0.f, 1.f, 0.f);
const vec3 YELLOW = vec3(1.f, 1.f, 0.f);
const vec3 RED = vec3(1.f, 0.f, 0.f);

const Vertex bot_left  = Vertex(vec3(-.75f, -.75f, 0.f), BLUE);
const Vertex bot_right = Vertex(vec3( .75f, -.75f, 0.f), GREEN);
const Vertex top_right = Vertex(vec3( .75f,  .75f, 0.f), YELLOW);
const Vertex top_left  = Vertex(vec3(-.75f,  .75f, 0.f), RED);

const Vertex Quad[6] = Vertex[6](
		bot_left, top_right, bot_right,
		bot_left, top_left, top_right
    );

const vec2 uvs[6] = vec2[6](
		vec2(0.f, 0.f), vec2(1.f, 1.f), vec2(1.f, 0.f),
		vec2(0.f, 0.f), vec2(0.f, 1.f), vec2(1.f, 1.f)
		);

void main()
{
    Vertex vert = Quad[gl_VertexIndex];
	outPos = vert.position;
	outColor = vert.color;
	outUv = uvs[gl_VertexIndex];
	gl_Position = vec4(vert.position, 1.f);
}
