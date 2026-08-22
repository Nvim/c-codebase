#ifndef MATH_H
#define MATH_H

#include "visibility.h"
#include <stdbool.h>

double const PI = 3.14159265358979323846f;
double const DEG2RAD = PI / 180.0f;
double const RAD2DEG = (180.0f / PI);

/**
 * r-prefix types are raw/array versions included in all union wrappers
 */
typedef float rvec2[2];
typedef float rvec3[3];
typedef float rvec4[4];

/**
 * Column-major arrays. Each column is a vector.
 */
typedef rvec3 rmat3[3];
typedef rvec4 rmat4[4];

typedef union vec2 {
	rvec2 raw;
	struct {
		float x, y;
	};
	struct {
		float r, g;
	};
} vec2;

typedef union vec3 {
	rvec3 raw;
	struct {
		float x, y, z;
	};
	struct {
		float r, g, b;
	};
} vec3;

typedef union vec4 {
	rvec4 raw;
	struct {
		float x, y, z, w;
	};
	struct {
		float r, g, b, a;
	};
} vec4;

typedef union mat3 {
	rmat3 raw;
	vec3 cols[3]; // 3 column vectors
	struct {
		float m0, m1, m2;
		float m3, m4, m5;
		float m6, m7, m8;
	};
} mat3;

typedef union mat4 {
	rmat4 raw;
	vec4 cols[4]; // 4 column vectors
	struct {
		float m0, m1, m2, m3;
		float m4, m5, m6, m7;
		float m8, m9, m10, m11;
		float m12, m13, m14, m15;
	};
} mat4;

LIB_EXPORT vec2 vec2_add(vec2 a, vec2 b);
LIB_EXPORT vec2 vec2_sub(vec2 a, vec2 b);
LIB_EXPORT vec2 vec2_mul(vec2 a, vec2 b);
LIB_EXPORT float vec2_dot(vec2 a, vec2 b);
LIB_EXPORT float vec2_length(vec2 v);
LIB_EXPORT vec2 vec2_reflect(vec2 a, vec2 b);
LIB_EXPORT vec2 vec2_scale(vec2 v, float scale);
LIB_EXPORT vec2 vec2_normalize(vec2 v);

LIB_EXPORT vec3 vec3_add(vec3 a, vec3 b);
LIB_EXPORT vec3 vec3_sub(vec3 a, vec3 b);
LIB_EXPORT vec3 vec3_mul(vec3 a, vec3 b);
LIB_EXPORT float vec3_dot(vec3 a, vec3 b);
LIB_EXPORT float vec3_length(vec3 v);
LIB_EXPORT vec3 vec3_cross(vec3 a, vec3 b);
LIB_EXPORT vec3 vec3_scale(vec3 v, float scale);
LIB_EXPORT vec3 vec3_normalize(vec3 v);

LIB_EXPORT vec4 vec4_add(vec4 a, vec4 b);
LIB_EXPORT vec4 vec4_sub(vec4 a, vec4 b);
LIB_EXPORT vec4 vec4_mul(vec4 a, vec4 b);
LIB_EXPORT float vec4_dot(vec4 a, vec4 b);
LIB_EXPORT float vec4_length(vec4 v);
LIB_EXPORT vec4 vec4_scale(vec4 v, float scale);
LIB_EXPORT vec4 vec4_normalize(vec4 v);

LIB_EXPORT bool mat3_equal(mat3 a, mat3 b);
LIB_EXPORT mat3 mat3_zero(void);
LIB_EXPORT mat3 mat3_identity(void);
LIB_EXPORT mat3 mat3_add(mat3 a, mat3 b);
LIB_EXPORT mat3 mat3_sub(mat3 a, mat3 b);
LIB_EXPORT mat3 mat3_mul(mat3 a, mat3 b);
LIB_EXPORT vec3 mat3_mul_vec(mat3 m, vec3 v);
LIB_EXPORT mat3 mat3_scale(mat3 m, float scale);
// todo: transpose

LIB_EXPORT bool mat4_equal(mat4 a, mat4 b);
LIB_EXPORT mat4 mat4_zero(void);
LIB_EXPORT mat4 mat4_identity(void);
LIB_EXPORT mat4 mat4_add(mat4 a, mat4 b);
LIB_EXPORT mat4 mat4_sub(mat4 a, mat4 b);
LIB_EXPORT mat4 mat4_mul(mat4 a, mat4 b);
LIB_EXPORT vec4 mat4_mul_vec(mat4 m, vec4 v);
LIB_EXPORT mat4 mat4_scale(mat4 m, float scale);
// todo: transpose

LIB_EXPORT mat3 mat_scale_2d(float scale);
LIB_EXPORT mat3 mat_translate_2d(vec2 translation);
LIB_EXPORT mat3 mat_rotate_2d(float rad);

LIB_EXPORT mat4 mat_scale_3d(float scale);
LIB_EXPORT mat4 mat_translate_3d(vec3 translation);
LIB_EXPORT mat4 mat_rotate_3d(vec3 axis, float rad);

#endif //! MATH_H
