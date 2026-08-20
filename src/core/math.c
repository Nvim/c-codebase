#include <core/core.h>
#include <core/math.h>

vec2 vec2_add(vec2 a, vec2 b) {
	return (vec2){
		.x = a.x + b.x,
		.y = a.y + b.y,
	};
}

vec2 vec2_sub(vec2 a, vec2 b) {
	return (vec2){
		.x = a.x - b.x,
		.y = a.y - b.y,
	};
}

vec2 vec2_mul(vec2 a, vec2 b) {
	return (vec2){
		.x = a.x * b.x,
		.y = a.y * b.y,
	};
}

float vec2_dot(vec2 a, vec2 b) { return (a.x * b.x) + (a.y * b.y); }

// TODO
float vec2_length(vec2 v);

vec3 vec3_add(vec3 a, vec3 b) {
	return (vec3){
		.x = a.x + b.x,
		.y = a.y + b.y,
		.z = a.z + b.z,
	};
}

vec3 vec3_sub(vec3 a, vec3 b) {
	return (vec3){
		.x = a.x - b.x,
		.y = a.y - b.y,
		.z = a.z - b.z,
	};
}

vec3 vec3_mul(vec3 a, vec3 b) {
	return (vec3){
		.x = a.x * b.x,
		.y = a.y * b.y,
		.z = a.z * b.z,
	};
}

float vec3_dot(vec3 a, vec3 b) {
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

// TODO
float vec3_length(vec2 v);

vec3 vec3_cross(vec3 a, vec3 b) {
	return (vec3){
		.x = (a.y * b.z - a.z * b.y),
		.y = (a.z * b.x - a.x * b.z),
		.z = (a.x * b.y - a.y * b.x),
	};
}

vec4 vec4_add(vec4 a, vec4 b) {
	return (vec4){
		.x = a.x + b.x,
		.y = a.y + b.y,
		.z = a.z + b.z,
		.w = a.w + b.w,
	};
}

vec4 vec4_sub(vec4 a, vec4 b) {
	return (vec4){
		.x = a.x - b.x,
		.y = a.y - b.y,
		.z = a.z - b.z,
		.w = a.w - b.w,
	};
}

vec4 vec4_mul(vec4 a, vec4 b) {
	return (vec4){
		.x = a.x * b.x,
		.y = a.y * b.y,
		.z = a.z * b.z,
		.w = a.w * b.w,
	};
}

float vec4_dot(vec4 a, vec4 b) {
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
}

// TODO
float vec4_length(vec4 v);

vec4 vec4_scale(vec4 v, float scale) {
	return (vec4){
		.x = v.x * scale,
		.y = v.y * scale,
		.z = v.z * scale,
		.w = v.z * scale,
	};
}

mat3 mat3_zero(void) { return (mat3){0}; }

mat3 mat3_identity(void) {
	mat3 ret = {0};

	ret.raw[0][0] = 1.f;
	ret.raw[1][1] = 1.f;
	ret.raw[2][2] = 1.f;

	return ret;
}

mat3 mat3_add(mat3 a, mat3 b) {
	return (mat3){
		.m0 = a.m0 + b.m0,
		.m3 = a.m3 + b.m3,
		.m6 = a.m6 + b.m6,
		.m1 = a.m1 + b.m1,
		.m4 = a.m4 + b.m4,
		.m7 = a.m7 + b.m7,
		.m2 = a.m2 + b.m2,
		.m5 = a.m5 + b.m5,
		.m8 = a.m8 + b.m8,
	};
}

mat3 mat3_sub(mat3 a, mat3 b) {
	return (mat3){
		.m0 = a.m0 - b.m0,
		.m3 = a.m3 - b.m3,
		.m6 = a.m6 - b.m6,
		.m1 = a.m1 - b.m1,
		.m4 = a.m4 - b.m4,
		.m7 = a.m7 - b.m7,
		.m2 = a.m2 - b.m2,
		.m5 = a.m5 - b.m5,
		.m8 = a.m8 - b.m8,
	};
}

// TODO
mat3 mat3_mul(UNUSED mat3 a, UNUSED mat3 b) { return (mat3){0}; }

mat3 mat3_scale(mat3 m, float scale) {
	return (mat3){
		.m0 = m.m0 * scale,
		.m3 = m.m3 * scale,
		.m6 = m.m6 * scale,
		.m1 = m.m1 * scale,
		.m4 = m.m4 * scale,
		.m7 = m.m7 * scale,
		.m2 = m.m2 * scale,
		.m5 = m.m5 * scale,
		.m8 = m.m8 * scale,
	};
}

mat4 mat4_zero(void) { return (mat4){0}; }

mat4 mat4_identity(void) {
	mat4 ret = {0};

	ret.raw[0][0] = 1.f;
	ret.raw[1][1] = 1.f;
	ret.raw[2][2] = 1.f;
	ret.raw[3][3] = 1.f;

	return ret;
}

mat4 mat4_add(UNUSED mat4 a, UNUSED mat4 b) {
	return (mat4){
		.m0 = a.m0 + b.m0,
		.m4 = a.m4 + b.m4,
		.m8 = a.m8 + b.m8,
		.m12 = a.m12 + b.m12,

		.m1 = a.m1 + b.m1,
		.m5 = a.m5 + b.m5,
		.m9 = a.m9 + b.m9,
		.m13 = a.m13 + b.m13,

		.m2 = a.m2 + b.m2,
		.m6 = a.m6 + b.m6,
		.m10 = a.m10 + b.m10,
		.m14 = a.m14 + b.m14,

		.m3 = a.m3 + b.m3,
		.m7 = a.m7 + b.m7,
		.m11 = a.m11 + b.m11,
		.m15 = a.m15 + b.m15,
	};
}

mat4 mat4_sub(UNUSED mat4 a, UNUSED mat4 b) {
	return (mat4){
		.m0 = a.m0 - b.m0,
		.m4 = a.m4 - b.m4,
		.m8 = a.m8 - b.m8,
		.m12 = a.m12 - b.m12,

		.m1 = a.m1 - b.m1,
		.m5 = a.m5 - b.m5,
		.m9 = a.m9 - b.m9,
		.m13 = a.m13 - b.m13,

		.m2 = a.m2 - b.m2,
		.m6 = a.m6 - b.m6,
		.m10 = a.m10 - b.m10,
		.m14 = a.m14 - b.m14,

		.m3 = a.m3 - b.m3,
		.m7 = a.m7 - b.m7,
		.m11 = a.m11 - b.m11,
		.m15 = a.m15 - b.m15,
	};
}

// TODO
mat4 mat4_mul(UNUSED mat4 a, UNUSED mat4 b) { return (mat4){0}; }

mat4 mat4_scale(mat4 m, float scale) {
	return (mat4){
		.m0 = m.m0 * scale,
		.m1 = m.m1 * scale,
		.m2 = m.m2 * scale,
		.m3 = m.m3 * scale,
		.m4 = m.m4 * scale,
		.m5 = m.m5 * scale,
		.m6 = m.m6 * scale,
		.m7 = m.m7 * scale,
		.m8 = m.m8 * scale,
		.m9 = m.m9 * scale,
		.m10 = m.m10 * scale,
		.m11 = m.m12 * scale,
		.m12 = m.m12 * scale,
		.m13 = m.m13 * scale,
		.m14 = m.m14 * scale,
		.m15 = m.m15 * scale,
	};
}
