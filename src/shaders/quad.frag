#version 460

layout (location=0) out vec4 outFragColor;
layout (location=0) in vec3 inPos;
layout (location=1) in vec3 inCol;

void main() {
	outFragColor = vec4(inCol, 1.f);
}
