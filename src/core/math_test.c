#include <core/core.h>
#include <core/math.h>

#include <unity.h>

void setUp(void) {}

void tearDown(void) {}

void test_mat3_equal(void) {
	mat3 a = (mat3){
		.cols =
			{
				{{1.f, 4.f, 2.f}},
				{{8.f, 3.f, 2.f}},
				{{3.f, 1.f, 4.f}},
			},
	};

	mat3 b = (mat3){
		.m0 = 1.f, .m1 = 4.f, .m2 = 2.f, // NOLINT
		.m3 = 8.f, .m4 = 3.f, .m5 = 2.f, // NOLINT
		.m6 = 3.f, .m7 = 1.f, .m8 = 4.f, // NOLINT
	};

	TEST_ASSERT_TRUE(mat3_equal(a, b));
}

void test_mat3_add(void) {
	mat3 a = mat3_identity();
	mat3 b = {
		.cols =
			{
				{{1.f, 1.f, 1.f}},
				{{0.f, 0.f, 0.f}},
				{{3.f, 3.f, 3.f}},
			},
	};

	mat3 expect = {
		.cols =
			{
				{{2.f, 1.f, 1.f}},
				{{0.f, 1.f, 0.f}},
				{{3.f, 3.f, 4.f}},
			},
	};

	TEST_ASSERT_TRUE(mat3_equal(expect, mat3_add(a, b)));
}

void test_mat3_mul_identity(void) {
	mat3 a = mat3_identity();
	mat3 b = mat3_identity();

	mat3 res = mat3_mul(a, b);
	TEST_ASSERT_TRUE(mat3_equal(a, res));
}

void test_mat3_mul_identity_other(void) {
	mat3 a = (mat3){
		.cols =
			{
				{{1.f, 4.f, 2.f}},
				{{8.f, 3.f, 2.f}},
				{{3.f, 1.f, 4.f}},
			},
	};

	mat3 b = mat3_identity();
	mat3 res = mat3_mul(a, b);
	TEST_ASSERT_TRUE(mat3_equal(a, res));
}

void test_mat3_mul_other(void) {
	mat3 a = (mat3){
		.cols =
			{
				{{1.f, 4.f, 2.f}},
				{{8.f, 3.f, 2.f}},
				{{3.f, 1.f, 4.f}},
			},
	};

	mat3 b = (mat3){
		.cols =
			{
				{{2.f, 1.f, 3.f}},
				{{4.f, 6.f, 4.f}},
				{{11.f, 1.f, 3.f}},
			},
	};

	mat3 expect = (mat3){
		.cols =
			{
				{{19.f, 14.f, 18.f}},
				{{64.f, 38.f, 36.f}},
				{{28.f, 50.f, 36.f}},
			},
	};

	mat3 res = mat3_mul(a, b);
	TEST_ASSERT_TRUE(mat3_equal(expect, res));
}

void test_mat3_scale_2d(void) {
	mat3 expect = (mat3){
		.m0 = 2.f,
		.m4 = 2.f,
		.m8 = 1.f,
	};

	TEST_ASSERT_TRUE(mat3_equal(expect, mat_scale_2d(2.f)));
}

void test_mat3_translate_2d(void) {
	mat3 expect = mat3_identity();
	expect.m6 = 3.f;
	expect.m7 = 4.f;

	TEST_ASSERT_TRUE(
		mat3_equal(expect, mat_translate_2d((vec2){.x = 3.f, .y = 4.f})));
}

void test_mat3_rotate_2d(void) {
	vec3 result = mat3_mul_vec(mat_rotate_2d((float)(PI / 2.0)),
							   (vec3){.x = 1.f, .y = 0.f, .z = 1.f});

	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.f, result.x);
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.f, result.y);
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.f, result.z);
}

void test_mat3_mul_vec(void) {
	mat3 m = (mat3){
		.cols =
			{
				{{1.f, 4.f, 7.f}},
				{{2.f, 5.f, 8.f}},
				{{3.f, 6.f, 9.f}},
			},
	};
	vec3 result = mat3_mul_vec(m, (vec3){.x = 1.f, .y = 2.f, .z = 3.f});

	TEST_ASSERT_EQUAL_FLOAT(14.f, result.x);
	TEST_ASSERT_EQUAL_FLOAT(32.f, result.y);
	TEST_ASSERT_EQUAL_FLOAT(50.f, result.z);
}

void test_mat4_equal(void) {
	mat4 a = (mat4){
		.raw =
			{
				{1.f, 4.f, 2.f, 5.f},
				{8.f, 3.f, 2.f, 6.f},
				{3.f, 1.f, 4.f, 7.f},
				{9.f, 5.f, 8.f, 2.f},
			},
	};

	mat4 b = (mat4){
		.m0 = 1.f,
		.m1 = 4.f,
		.m2 = 2.f,
		.m3 = 5.f,
		.m4 = 8.f,
		.m5 = 3.f,
		.m6 = 2.f,
		.m7 = 6.f,
		.m8 = 3.f,
		.m9 = 1.f,
		.m10 = 4.f,
		.m11 = 7.f,
		.m12 = 9.f,
		.m13 = 5.f,
		.m14 = 8.f,
		.m15 = 2.f,
	};

	TEST_ASSERT_TRUE(mat4_equal(a, b));
}

void test_mat4_add(void) {
	mat4 a = mat4_identity();
	mat4 b = (mat4){
		.raw =
			{
				{1.f, 1.f, 1.f, 1.f},
				{0.f, 0.f, 0.f, 0.f},
				{3.f, 3.f, 3.f, 3.f},
				{4.f, 4.f, 4.f, 4.f},
			},
	};

	mat4 expect = (mat4){
		.raw =
			{
				{2.f, 1.f, 1.f, 1.f},
				{0.f, 1.f, 0.f, 0.f},
				{3.f, 3.f, 4.f, 3.f},
				{4.f, 4.f, 4.f, 5.f},
			},
	};

	TEST_ASSERT_TRUE(mat4_equal(expect, mat4_add(a, b)));
}

void test_mat4_mul_identity(void) {
	mat4 a = mat4_identity();
	mat4 b = mat4_identity();

	mat4 res = mat4_mul(a, b);
	TEST_ASSERT_TRUE(mat4_equal(a, res));
}

void test_mat4_mul_identity_other(void) {
	mat4 a = (mat4){
		.raw =
			{
				{1.f, 4.f, 2.f, 5.f},
				{8.f, 3.f, 2.f, 6.f},
				{3.f, 1.f, 4.f, 7.f},
				{9.f, 5.f, 8.f, 2.f},
			},
	};

	mat4 b = mat4_identity();
	mat4 res = mat4_mul(a, b);
	TEST_ASSERT_TRUE(mat4_equal(a, res));
}

void test_mat4_mul_other(void) {
	mat4 a = (mat4){
		.raw =
			{
				{1.f, 4.f, 2.f, 5.f},
				{8.f, 3.f, 2.f, 6.f},
				{3.f, 1.f, 4.f, 7.f},
				{9.f, 5.f, 8.f, 2.f},
			},
	};

	mat4 b = (mat4){
		.raw =
			{
				{2.f, 1.f, 3.f, 4.f},
				{4.f, 6.f, 4.f, 2.f},
				{11.f, 1.f, 3.f, 5.f},
				{7.f, 2.f, 6.f, 1.f},
			},
	};

	mat4 expect = (mat4){
		.raw =
			{
				{55.f, 34.f, 50.f, 45.f},
				{82.f, 48.f, 52.f, 88.f},
				{73.f, 75.f, 76.f, 92.f},
				{50.f, 45.f, 50.f, 91.f},
			},
	};

	mat4 res = mat4_mul(a, b);
	TEST_ASSERT_TRUE(mat4_equal(expect, res));
}

void test_mat4_scale_3d(void) {
	mat4 expect = (mat4){
		.m0 = 2.f,
		.m5 = 2.f,
		.m10 = 2.f,
		.m15 = 1.f,
	};

	TEST_ASSERT_TRUE(mat4_equal(expect, mat_scale_3d(2.f)));
}

void test_mat4_translate_3d(void) {
	mat4 expect = mat4_identity();
	expect.m12 = 4.f;
	expect.m13 = 5.f;
	expect.m14 = 6.f;

	TEST_ASSERT_TRUE(mat4_equal(
		expect, mat_translate_3d((vec3){.x = 4.f, .y = 5.f, .z = 6.f})));
}

void test_mat4_rotate_3d(void) {
	vec4 result = mat4_mul_vec(
		mat_rotate_3d((vec3){.x = 0.f, .y = 0.f, .z = 1.f}, (float)(PI / 2.0)),
		(vec4){.x = 1.f, .y = 0.f, .z = 0.f, .w = 1.f});

	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.f, result.x);
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.f, result.y);
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.f, result.z);
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.f, result.w);
}

void test_mat4_mul_vec(void) {
	mat4 m = (mat4){
		.raw =
			{
				{1.f, 2.f, 3.f, 4.f},
				{5.f, 6.f, 7.f, 8.f},
				{9.f, 10.f, 11.f, 12.f},
				{13.f, 14.f, 15.f, 16.f},
			},
	};
	vec4 result =
		mat4_mul_vec(m, (vec4){.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f});

	TEST_ASSERT_EQUAL_FLOAT(90.f, result.x);
	TEST_ASSERT_EQUAL_FLOAT(100.f, result.y);
	TEST_ASSERT_EQUAL_FLOAT(110.f, result.z);
	TEST_ASSERT_EQUAL_FLOAT(120.f, result.w);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_mat3_equal);
	RUN_TEST(test_mat3_add);
	RUN_TEST(test_mat3_mul_identity);
	RUN_TEST(test_mat3_mul_identity_other);
	RUN_TEST(test_mat3_mul_other);
	RUN_TEST(test_mat3_scale_2d);
	RUN_TEST(test_mat3_translate_2d);
	RUN_TEST(test_mat3_rotate_2d);
	RUN_TEST(test_mat3_mul_vec);
	RUN_TEST(test_mat4_equal);
	RUN_TEST(test_mat4_add);
	RUN_TEST(test_mat4_mul_identity);
	RUN_TEST(test_mat4_mul_identity_other);
	RUN_TEST(test_mat4_mul_other);
	RUN_TEST(test_mat4_scale_3d);
	RUN_TEST(test_mat4_translate_3d);
	RUN_TEST(test_mat4_rotate_3d);
	RUN_TEST(test_mat4_mul_vec);

	return UNITY_END();
}
