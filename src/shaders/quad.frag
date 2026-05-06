#version 460

layout(set=0, binding=0) uniform sampler2D TexWhite;

layout (location=0) out vec4 outFragColor;
layout (location=0) in vec3 inPos;
layout (location=1) in vec3 inCol;
layout (location=2) in vec2 inUv;

void main() {
	vec3 col = texture(TexWhite, inUv).rgb;
	// vec3 col = inCol;
	outFragColor = vec4(col, 1.f);
}
