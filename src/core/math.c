#include <core/core.h>
#include <core/math.h>

void vec2_add(vec2* out, vec2 a, vec2 b) {
	out->x = a.x + b.x;
	out->y = a.y + b.y;
}

void vec2_sub(vec2* out, vec2 a, vec2 b) {
	out->x = a.x - b.x;
	out->y = a.y - b.y;
}

void vec2_mul(vec2* out, vec2 a, vec2 b) {
	out->x = a.x * b.x;
	out->y = a.y * b.y;
}

void vec2_dot(UNUSED vec2* out, UNUSED vec2 a, UNUSED vec2 b) { return; }
void vec2_cross(UNUSED vec2* out, UNUSED vec2 a, UNUSED vec2 b) { return; }

void vec3_add(vec3* out, vec3 a, vec3 b) {
	out->x = a.x + b.x;
	out->y = a.y + b.y;
	out->z = a.z + b.z;
}

void vec3_sub(vec3* out, vec3 a, vec3 b) {
	out->x = a.x - b.x;
	out->y = a.y - b.y;
	out->z = a.z - b.z;
}

void vec3_mul(vec3* out, vec3 a, vec3 b) {
	out->x = a.x * b.x;
	out->y = a.y * b.y;
	out->z = a.z * b.z;
}

void vec3_dot(UNUSED vec3* out, UNUSED vec3 a, UNUSED vec3 b) { return; }
void vec3_cross(UNUSED vec3* out, UNUSED vec3 a, UNUSED vec3 b) { return; }

void vec4_add(vec4* out, vec4 a, vec4 b) {
	out->x = a.x + b.x;
	out->y = a.y + b.y;
	out->z = a.z + b.z;
	out->w = a.w + b.w;
}

void vec4_sub(vec4* out, vec4 a, vec4 b) {
	out->x = a.x - b.x;
	out->y = a.y - b.y;
	out->z = a.z - b.z;
	out->w = a.w - b.w;
}

void vec4_mul(vec4* out, vec4 a, vec4 b) {
	out->x = a.x * b.x;
	out->y = a.y * b.y;
	out->z = a.z * b.z;
	out->w = a.w * b.w;
}

void vec4_dot(UNUSED vec4* out, UNUSED vec4 a, UNUSED vec4 b) { return; }
void vec4_cross(UNUSED vec4* out, UNUSED vec4 a, UNUSED vec4 b) { return; }

void mat3_add(UNUSED mat3* out, UNUSED mat3 a, UNUSED mat3 b) { return; }
void mat3_sub(UNUSED mat3* out, UNUSED mat3 a, UNUSED mat3 b) { return; }
void mat3_mul(UNUSED mat3* out, UNUSED mat3 a, UNUSED mat3 b) { return; }
void mat4_add(UNUSED mat4* out, UNUSED mat4 a, UNUSED mat4 b) { return; }
void mat4_sub(UNUSED mat4* out, UNUSED mat4 a, UNUSED mat4 b) { return; }
void mat4_mul(UNUSED mat4* out, UNUSED mat4 a, UNUSED mat4 b) { return; }
