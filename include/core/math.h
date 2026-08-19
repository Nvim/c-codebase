#ifndef MATH_H
#define MATH_H

#include "visibility.h"

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
	vec3 cols; // 3 column vectors
	struct {
		float m0, m3, m6;
		float m1, m4, m7;
		float m2, m5, m8;
	};
} mat3;

typedef union mat4 {
	rmat4 raw;
	vec4 cols; // 4 column vectors
	struct {
		float m0, m4, m8, m12;
		float m1, m5, m9, m13;
		float m2, m6, m10, m14;
		float m3, m7, m11, m15;
	};
} mat4;

// [TODO]: out params vs return values
LIB_EXPORT void vec2_add(vec2* out, vec2 a, vec2 b);
LIB_EXPORT void vec2_sub(vec2* out, vec2 a, vec2 b);
LIB_EXPORT void vec2_mul(vec2* out, vec2 a, vec2 b);
LIB_EXPORT void vec2_dot(vec2* out, vec2 a, vec2 b);
LIB_EXPORT void vec2_cross(vec2* out, vec2 a, vec2 b);
LIB_EXPORT void vec2_reflect(vec2* out, vec2 a, vec2 b);
LIB_EXPORT void vec2_scale(vec2* out, float scale);

LIB_EXPORT void vec3_add(vec3* out, vec3 a, vec3 b);
LIB_EXPORT void vec3_sub(vec3* out, vec3 a, vec3 b);
LIB_EXPORT void vec3_mul(vec3* out, vec3 a, vec3 b);
LIB_EXPORT void vec3_dot(vec3* out, vec3 a, vec3 b);
LIB_EXPORT void vec3_cross(vec3* out, vec3 a, vec3 b);

LIB_EXPORT void vec4_add(vec4* out, vec4 a, vec4 b);
LIB_EXPORT void vec4_sub(vec4* out, vec4 a, vec4 b);
LIB_EXPORT void vec4_mul(vec4* out, vec4 a, vec4 b);
LIB_EXPORT void vec4_dot(vec4* out, vec4 a, vec4 b);
LIB_EXPORT void vec4_cross(vec4* out, vec4 a, vec4 b);

LIB_EXPORT void mat3_add(mat3* out, mat3 a, mat3 b);
LIB_EXPORT void mat3_sub(mat3* out, mat3 a, mat3 b);
LIB_EXPORT void mat3_mul(mat3* out, mat3 a, mat3 b);

LIB_EXPORT void mat4_add(mat4* out, mat4 a, mat4 b);
LIB_EXPORT void mat4_sub(mat4* out, mat4 a, mat4 b);
LIB_EXPORT void mat4_mul(mat4* out, mat4 a, mat4 b);

#endif //! MATH_H
