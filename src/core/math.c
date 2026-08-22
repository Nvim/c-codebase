#include <core/core.h>
#include <core/math.h>
#include <math.h>

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

float vec2_length(vec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }

// TODO
vec2 vec2_reflect(vec2 a, vec2 b);

vec2 vec2_scale(vec2 v, float scale) {
	return (vec2){.x = v.x * scale, .y = v.y * scale};
}

vec2 vec2_normalize(vec2 v) {
	float len = vec2_length(v);
	if (len == 0.f) { return (vec2){{0.f}}; }
	return vec2_scale(v, 1 / vec2_length(v));
}

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

float vec3_length(vec3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }

vec3 vec3_cross(vec3 a, vec3 b) {
	return (vec3){
		.x = (a.y * b.z - a.z * b.y),
		.y = (a.z * b.x - a.x * b.z),
		.z = (a.x * b.y - a.y * b.x),
	};
}

vec3 vec3_scale(vec3 v, float scale) {
	return (vec3){.x = v.x * scale, .y = v.y * scale, .z = v.z * scale};
}

vec3 vec3_normalize(vec3 v) {
	float len = vec3_length(v);
	if (len == 0.f) { return (vec3){{0.f}}; }
	return vec3_scale(v, 1 / vec3_length(v));
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

float vec4_length(vec4 v) {
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

vec4 vec4_scale(vec4 v, float scale) {
	return (vec4){
		.x = v.x * scale,
		.y = v.y * scale,
		.z = v.z * scale,
		.w = v.z * scale,
	};
}

vec4 vec4_normalize(vec4 v) {
	float len = vec4_length(v);
	if (len == 0.f) { return (vec4){{0.f}}; }
	return vec4_scale(v, 1 / vec4_length(v));
}

bool mat3_equal(mat3 a, mat3 b) {
	// clang-format off
	return (
		a.m0 == b.m0 &&
		a.m3 == b.m3 &&
		a.m6 == b.m6 &&
		a.m1 == b.m1 &&
		a.m4 == b.m4 &&
		a.m7 == b.m7 &&
		a.m2 == b.m2 &&
		a.m5 == b.m5 &&
		a.m8 == b.m8
	);
	// clang-format on
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

mat3 mat3_mul(mat3 a, mat3 b) {
	mat3 ret = {0};

	// col 0
	ret.m0 = a.m0 * b.m0 + a.m3 * b.m1 + a.m6 * b.m2;
	ret.m1 = a.m1 * b.m0 + a.m4 * b.m1 + a.m7 * b.m2;
	ret.m2 = a.m2 * b.m0 + a.m5 * b.m1 + a.m8 * b.m2;

	// col 1
	ret.m3 = a.m0 * b.m3 + a.m3 * b.m4 + a.m6 * b.m5;
	ret.m4 = a.m1 * b.m3 + a.m4 * b.m4 + a.m7 * b.m5;
	ret.m5 = a.m2 * b.m3 + a.m5 * b.m4 + a.m8 * b.m5;

	// col 2
	ret.m6 = a.m0 * b.m6 + a.m3 * b.m7 + a.m6 * b.m8;
	ret.m7 = a.m1 * b.m6 + a.m4 * b.m7 + a.m7 * b.m8;
	ret.m8 = a.m2 * b.m6 + a.m5 * b.m7 + a.m8 * b.m8;

	return ret;
}

vec3 mat3_mul_vec(mat3 m, vec3 v) {
	return (vec3){
		.x = m.m0 * v.x + m.m3 * v.y + m.m6 * v.z,
		.y = m.m1 * v.x + m.m4 * v.y + m.m7 * v.z,
		.z = m.m2 * v.x + m.m5 * v.y + m.m8 * v.z,
	};
}

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

bool mat4_equal(mat4 a, mat4 b) {
	// clang-format off
	return (
			a.m0  == b.m0 &&
			a.m4  == b.m4 &&
			a.m8  == b.m8 &&
			a.m12 == b.m12 &&
			a.m1  == b.m1 &&
			a.m5  == b.m5 &&
			a.m9  == b.m9 &&
			a.m13 == b.m13 &&
			a.m2  == b.m2 &&
			a.m6  == b.m6 &&
			a.m10 == b.m10 &&
			a.m14 == b.m14 &&
			a.m3  == b.m3 &&
			a.m7  == b.m7 &&
			a.m11 == b.m11 &&
			a.m15 == b.m15
	);
	// clang-format on
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

mat4 mat4_add(mat4 a, mat4 b) {
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

mat4 mat4_sub(mat4 a, mat4 b) {
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

mat4 mat4_mul(mat4 a, mat4 b) {
	mat4 ret = {0};

	// col 0
	ret.m0 = a.m0 * b.m0 + a.m4 * b.m1 + a.m8 * b.m2 + a.m12 * b.m3;
	ret.m1 = a.m1 * b.m0 + a.m5 * b.m1 + a.m9 * b.m2 + a.m13 * b.m3;
	ret.m2 = a.m2 * b.m0 + a.m6 * b.m1 + a.m10 * b.m2 + a.m14 * b.m3;
	ret.m3 = a.m3 * b.m0 + a.m7 * b.m1 + a.m11 * b.m2 + a.m15 * b.m3;

	// col 1
	ret.m4 = a.m0 * b.m4 + a.m4 * b.m5 + a.m8 * b.m6 + a.m12 * b.m7;
	ret.m5 = a.m1 * b.m4 + a.m5 * b.m5 + a.m9 * b.m6 + a.m13 * b.m7;
	ret.m6 = a.m2 * b.m4 + a.m6 * b.m5 + a.m10 * b.m6 + a.m14 * b.m7;
	ret.m7 = a.m3 * b.m4 + a.m7 * b.m5 + a.m11 * b.m6 + a.m15 * b.m7;

	// col 2
	ret.m8 = a.m0 * b.m8 + a.m4 * b.m9 + a.m8 * b.m10 + a.m12 * b.m11;
	ret.m9 = a.m1 * b.m8 + a.m5 * b.m9 + a.m9 * b.m10 + a.m13 * b.m11;
	ret.m10 = a.m2 * b.m8 + a.m6 * b.m9 + a.m10 * b.m10 + a.m14 * b.m11;
	ret.m11 = a.m3 * b.m8 + a.m7 * b.m9 + a.m11 * b.m10 + a.m15 * b.m11;

	// col 3
	ret.m12 = a.m0 * b.m12 + a.m4 * b.m13 + a.m8 * b.m14 + a.m12 * b.m15;
	ret.m13 = a.m1 * b.m12 + a.m5 * b.m13 + a.m9 * b.m14 + a.m13 * b.m15;
	ret.m14 = a.m2 * b.m12 + a.m6 * b.m13 + a.m10 * b.m14 + a.m14 * b.m15;
	ret.m15 = a.m3 * b.m12 + a.m7 * b.m13 + a.m11 * b.m14 + a.m15 * b.m15;

	return ret;
}

vec4 mat4_mul_vec(mat4 m, vec4 v) {
	return (vec4){
		.x = m.m0 * v.x + m.m4 * v.y + m.m8 * v.z + m.m12 * v.w,
		.y = m.m1 * v.x + m.m5 * v.y + m.m9 * v.z + m.m13 * v.w,
		.z = m.m2 * v.x + m.m6 * v.y + m.m10 * v.z + m.m14 * v.w,
		.w = m.m3 * v.x + m.m7 * v.y + m.m11 * v.z + m.m15 * v.w,
	};
}

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

mat3 mat_scale_2d(float scale) {
	mat3 ret = {0};

	ret.m0 = scale;
	ret.m4 = scale;
	ret.m8 = 1.f;

	return ret;
}

mat3 mat_translate_2d(vec2 translation) {
	mat3 ret = mat3_identity();

	ret.m6 = translation.x;
	ret.m7 = translation.y;

	return ret;
}

mat3 mat_rotate_2d(float rad) {
	float c = cosf(rad);
	float s = sinf(rad);

	return (mat3){
		.m0 = c,   .m1 = s,   .m2 = 0.f, // NOLINT
		.m3 = -s,  .m4 = c,   .m5 = 0.f, // NOLINT
		.m6 = 0.f, .m7 = 0.f, .m8 = 1.f, // NOLINT
	};
}

mat4 mat_scale_3d(float scale) {
	mat4 ret = {0};

	ret.m0 = scale;
	ret.m5 = scale;
	ret.m10 = scale;
	ret.m15 = 1.f;

	return ret;
}

mat4 mat_translate_3d(vec3 translation) {
	mat4 ret = mat4_identity();

	ret.m12 = translation.x;
	ret.m13 = translation.y;
	ret.m14 = translation.z;

	return ret;
}

mat4 mat_rotate_3d(vec3 axis, float rad) {
	float axis_length = vec3_length(axis);
	if (axis_length == 0.f) { return mat4_identity(); }

	float x = axis.x / axis_length;
	float y = axis.y / axis_length;
	float z = axis.z / axis_length;
	float c = cosf(rad);
	float s = sinf(rad);
	float t = 1.f - c;

	return (mat4){
		.m0 = t * x * x + c,
		.m1 = t * x * y + s * z,
		.m2 = t * x * z - s * y,
		.m3 = 0.f,

		.m4 = t * x * y - s * z,
		.m5 = t * y * y + c,
		.m6 = t * y * z + s * x,
		.m7 = 0.f,

		.m8 = t * x * z + s * y,
		.m9 = t * y * z - s * x,
		.m10 = t * z * z + c,
		.m11 = 0.f,

		.m12 = 0.f,
		.m13 = 0.f,
		.m14 = 0.f,
		.m15 = 1.f,
	};
}
