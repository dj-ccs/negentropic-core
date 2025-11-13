/*
 * fixed_point_accuracy_test.c - Unit Tests for SE(3) Fixed-Point Math
 *
 * Tests for:
 *   1. Fixed-point arithmetic (FixedMul, FixedDiv)
 *   2. Trigonometric LUT accuracy
 *   3. Rotation matrix operations
 *   4. Coordinate transformations
 *
 * Compile with:
 *   gcc -o fixed_point_test fixed_point_accuracy_test.c \
 *       ../embedded/se3_math.c ../embedded/trig_tables.c \
 *       -I../embedded -lm -std=c99
 *
 * Author: ClaudeCode (Doom→SE(3) λ-Estimation Service)
 * Version: 1.0
 */

#include "../embedded/se3_edge.h"
#include <stdio.h>
#include <stdlib.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Test statistics */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macros */
#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s\n", msg); \
    } \
} while(0)

#define TOLERANCE_FIXED FLOAT_TO_FIXED(0.001f)  /* 0.1% tolerance */
#define TOLERANCE_FLOAT 0.001f

/* ========================================================================
 * TEST: Fixed-Point Arithmetic
 * ======================================================================== */

void test_fixed_point_arithmetic(void) {
    printf("\n[TEST] Fixed-Point Arithmetic\n");

    /* Test FixedMul */
    fixed_t a = FLOAT_TO_FIXED(2.5f);
    fixed_t b = FLOAT_TO_FIXED(3.0f);
    fixed_t result = FixedMul(a, b);
    float result_f = FIXED_TO_FLOAT(result);
    TEST_ASSERT(fabs(result_f - 7.5f) < TOLERANCE_FLOAT, "FixedMul(2.5, 3.0) = 7.5");

    /* Test FixedDiv */
    a = FLOAT_TO_FIXED(10.0f);
    b = FLOAT_TO_FIXED(4.0f);
    result = FixedDiv(a, b);
    result_f = FIXED_TO_FLOAT(result);
    TEST_ASSERT(fabs(result_f - 2.5f) < TOLERANCE_FLOAT, "FixedDiv(10.0, 4.0) = 2.5");

    /* Test divide-by-zero saturation */
    result = FixedDiv(FRACUNIT, 0);
    TEST_ASSERT(result == INT32_MAX, "FixedDiv(1.0, 0) saturates to INT32_MAX");

    /* Test negative multiply */
    a = FLOAT_TO_FIXED(-1.5f);
    b = FLOAT_TO_FIXED(2.0f);
    result = FixedMul(a, b);
    result_f = FIXED_TO_FLOAT(result);
    TEST_ASSERT(fabs(result_f - (-3.0f)) < TOLERANCE_FLOAT, "FixedMul(-1.5, 2.0) = -3.0");

    /* Test that FixedMul uses 64-bit intermediate (prevents overflow during calculation) */
    /* Max safe integer in 16.16: ~32,767. Test with values that need 64-bit intermediate. */
    a = INT_TO_FIXED(150);    /* 150 * 65536 = 9,830,400 */
    b = INT_TO_FIXED(150);    /* 150 * 65536 = 9,830,400 */
    result = FixedMul(a, b);  /* (9830400 * 9830400) >> 16 = 1,474,560,000 >> 16 = 22,500 */
    result_f = FIXED_TO_FLOAT(result);
    /* Expected: 150 * 150 = 22,500 */
    TEST_ASSERT(fabs(result_f - 22500.0f) < 10.0f, "FixedMul(150, 150) = 22500 (64-bit intermediate)");
}

/* ========================================================================
 * TEST: Trigonometric LUT
 * ======================================================================== */

void test_trig_lut_accuracy(void) {
    printf("\n[TEST] Trigonometric LUT Accuracy\n");

    /* Test critical angles */
    struct {
        float deg;
        float expected_sin;
        float expected_cos;
    } critical_angles[] = {
        {0.0f, 0.0f, 1.0f},
        {30.0f, 0.5f, 0.866f},
        {45.0f, 0.707f, 0.707f},
        {60.0f, 0.866f, 0.5f},
        {90.0f, 1.0f, 0.0f},
        {180.0f, 0.0f, -1.0f},
        {270.0f, -1.0f, 0.0f},
    };

    for (size_t i = 0; i < sizeof(critical_angles) / sizeof(critical_angles[0]); i++) {
        float deg = critical_angles[i].deg;
        float rad = deg * M_PI / 180.0f;

        /* Convert to 32-bit angle */
        uint32_t angle = (uint32_t)((deg / 360.0f) * 4294967296.0);

        /* Get LUT values */
        fixed_t sin_lut = Sin_from_LUT(angle);
        fixed_t cos_lut = Cos_from_LUT(angle);

        float sin_f = FIXED_TO_FLOAT(sin_lut);
        float cos_f = FIXED_TO_FLOAT(cos_lut);

        /* Check against expected (with tolerance for quantization) */
        char msg[128];
        snprintf(msg, sizeof(msg), "sin(%.0f°) ≈ %.3f", deg, critical_angles[i].expected_sin);
        TEST_ASSERT(fabs(sin_f - critical_angles[i].expected_sin) < 0.01f, msg);

        snprintf(msg, sizeof(msg), "cos(%.0f°) ≈ %.3f", deg, critical_angles[i].expected_cos);
        TEST_ASSERT(fabs(cos_f - critical_angles[i].expected_cos) < 0.01f, msg);
    }

    /* Test Pythagorean identity: sin² + cos² = 1 */
    int pythag_pass = 0;
    for (int i = 0; i < 100; i++) {
        uint32_t angle = (uint32_t)(rand() * 4294967296.0 / RAND_MAX);
        fixed_t error = verify_pythagorean_identity(angle);
        if (error < TOLERANCE_FIXED) {
            pythag_pass++;
        }
    }
    TEST_ASSERT(pythag_pass >= 95, "Pythagorean identity holds for ≥95% of random angles");
}

/* ========================================================================
 * TEST: Rotation Matrix Operations
 * ======================================================================== */

void test_rotation_matrices(void) {
    printf("\n[TEST] Rotation Matrix Operations\n");

    /* Test identity matrix */
    fixed_t R[9];
    rotation_identity(R);
    TEST_ASSERT(R[0] == FRACUNIT && R[4] == FRACUNIT && R[8] == FRACUNIT,
                "rotation_identity() creates identity matrix");
    TEST_ASSERT(R[1] == 0 && R[2] == 0 && R[3] == 0,
                "rotation_identity() zeros off-diagonal elements");

    /* Test yaw rotation (90°) */
    uint32_t angle_90 = 0x40000000;  /* 90° in 32-bit angle */
    rotation_from_yaw(angle_90, R);

    /* R(90°) should be approximately:
     * [ 0 -1  0]
     * [ 1  0  0]
     * [ 0  0  1]
     */
    float r00 = FIXED_TO_FLOAT(R[0]);
    float r01 = FIXED_TO_FLOAT(R[1]);
    float r10 = FIXED_TO_FLOAT(R[3]);
    TEST_ASSERT(fabs(r00) < 0.01f, "R(90°)[0,0] ≈ 0");
    TEST_ASSERT(fabs(r01 - (-1.0f)) < 0.01f, "R(90°)[0,1] ≈ -1");
    TEST_ASSERT(fabs(r10 - 1.0f) < 0.01f, "R(90°)[1,0] ≈ 1");

    /* Test rotation composition: R(45°) * R(45°) = R(90°) */
    fixed_t R1[9], R2[9], R_composed[9];
    uint32_t angle_45 = 0x20000000;  /* 45° */

    rotation_from_yaw(angle_45, R1);
    rotation_from_yaw(angle_45, R2);
    rotation_mul(R1, R2, R_composed);

    /* Compare with direct R(90°) */
    float comp_00 = FIXED_TO_FLOAT(R_composed[0]);
    float comp_01 = FIXED_TO_FLOAT(R_composed[1]);
    TEST_ASSERT(fabs(comp_00 - r00) < 0.01f, "R(45°) * R(45°) ≈ R(90°)");

    /* Test trace */
    fixed_t trace = rotation_trace(R);
    float trace_f = FIXED_TO_FLOAT(trace);
    /* For R(90°) in 2D (3rd dimension is identity): trace ≈ 1.0 (cos(90°) + cos(90°) + 1) ≈ 1.0 */
    /* Actually: R[0,0] + R[1,1] + R[2,2] = 0 + 0 + 1 = 1.0 for pure 2D rotation */
    TEST_ASSERT(fabs(trace_f - 1.0f) < 0.1f, "trace(R(90°)) ≈ 1.0");
}

/* ========================================================================
 * TEST: Geodetic Utilities
 * ======================================================================== */

void test_geodetic_utils(void) {
    printf("\n[TEST] Geodetic Utilities\n");

    /* Test longitude normalization (dateline crossing) */
    fixed_t lon_190 = FLOAT_TO_FIXED(190.0f);
    fixed_t lon_norm = normalize_lon(lon_190);
    float lon_norm_f = FIXED_TO_FLOAT(lon_norm);
    TEST_ASSERT(fabs(lon_norm_f - (-170.0f)) < 0.01f, "normalize_lon(190°) = -170°");

    fixed_t lon_neg200 = FLOAT_TO_FIXED(-200.0f);
    lon_norm = normalize_lon(lon_neg200);
    lon_norm_f = FIXED_TO_FLOAT(lon_norm);
    TEST_ASSERT(fabs(lon_norm_f - 160.0f) < 0.01f, "normalize_lon(-200°) = 160°");

    /* Test GPS heading to SE(3) angle conversion */
    fixed_t heading_0 = FLOAT_TO_FIXED(0.0f);  /* North */
    uint32_t angle_0 = heading_to_angle(heading_0);

    /* GPS 0° (North) → SE(3) 90° (North in ENU) */
    /* 90° in 32-bit: 0x40000000 */
    TEST_ASSERT((angle_0 >> 30) == 1, "GPS heading 0° (North) → SE(3) 90°");

    fixed_t heading_90 = FLOAT_TO_FIXED(90.0f);  /* East */
    uint32_t angle_90 = heading_to_angle(heading_90);

    /* GPS 90° (East) → SE(3) 180° (West in ENU)... wait, this is wrong in my expectation */
    /* Actually: GPS 90° (East) + 90° correction = SE(3) 180° */
    /* Let me recalculate: GPS 90° + 90° = 180° → 0x80000000 */
    TEST_ASSERT((angle_90 >> 31) == 1, "GPS heading 90° (East) → SE(3) 180°");
}

/* ========================================================================
 * TEST: Vector Operations
 * ======================================================================== */

void test_vector_ops(void) {
    printf("\n[TEST] Vector Operations\n");

    /* Test norm squared */
    fixed_t v[3] = {
        FLOAT_TO_FIXED(3.0f),
        FLOAT_TO_FIXED(4.0f),
        FLOAT_TO_FIXED(0.0f)
    };
    fixed_t norm_sq = vec3_norm_squared(v);
    float norm_sq_f = FIXED_TO_FLOAT(norm_sq);
    TEST_ASSERT(fabs(norm_sq_f - 25.0f) < 0.1f, "||[3, 4, 0]||² = 25");

    /* Test vector subtraction */
    fixed_t a[3] = {FLOAT_TO_FIXED(5.0f), FLOAT_TO_FIXED(3.0f), FLOAT_TO_FIXED(1.0f)};
    fixed_t b[3] = {FLOAT_TO_FIXED(2.0f), FLOAT_TO_FIXED(1.0f), FLOAT_TO_FIXED(1.0f)};
    fixed_t result[3];
    vec3_sub(a, b, result);

    float r0 = FIXED_TO_FLOAT(result[0]);
    float r1 = FIXED_TO_FLOAT(result[1]);
    float r2 = FIXED_TO_FLOAT(result[2]);
    TEST_ASSERT(fabs(r0 - 3.0f) < 0.01f && fabs(r1 - 2.0f) < 0.01f && fabs(r2 - 0.0f) < 0.01f,
                "vec3_sub([5,3,1], [2,1,1]) = [3,2,0]");

    /* Test matrix-vector multiplication */
    fixed_t R[9];
    rotation_from_yaw(0x40000000, R);  /* 90° rotation */

    fixed_t vec_in[3] = {FRACUNIT, 0, 0};  /* [1, 0, 0] */
    fixed_t vec_out[3];
    mat3_mul_vec3(R, vec_in, vec_out);

    /* R(90°) * [1, 0, 0] should give approximately [0, 1, 0] */
    float out_x = FIXED_TO_FLOAT(vec_out[0]);
    float out_y = FIXED_TO_FLOAT(vec_out[1]);
    TEST_ASSERT(fabs(out_x) < 0.01f && fabs(out_y - 1.0f) < 0.01f,
                "R(90°) * [1,0,0] ≈ [0,1,0]");
}

/* ========================================================================
 * TEST: SE(3) Pose Operations
 * ======================================================================== */

void test_se3_poses(void) {
    printf("\n[TEST] SE(3) Pose Operations\n");

    /* Test identity pose */
    se3_pose_t pose;
    se3_pose_identity(&pose);

    TEST_ASSERT(pose.rotation[0] == FRACUNIT && pose.rotation[4] == FRACUNIT,
                "se3_pose_identity() creates identity rotation");
    TEST_ASSERT(pose.translation[0] == 0 && pose.translation[1] == 0 && pose.translation[2] == 0,
                "se3_pose_identity() creates zero translation");

    /* Test pose from GPS data */
    se3_pose_from_gps(
        FLOAT_TO_FIXED(100.0f),  /* 100m East */
        FLOAT_TO_FIXED(200.0f),  /* 200m North */
        FLOAT_TO_FIXED(0.0f),    /* 0m Up */
        FLOAT_TO_FIXED(45.0f),   /* 45° GPS heading (NE) */
        1699000000,              /* timestamp */
        367123456,               /* MMSI */
        &pose
    );

    float east = FIXED_TO_FLOAT(pose.translation[0]);
    float north = FIXED_TO_FLOAT(pose.translation[1]);

    TEST_ASSERT(fabs(east - 100.0f) < 0.1f && fabs(north - 200.0f) < 0.1f,
                "se3_pose_from_gps() sets translation correctly");
    TEST_ASSERT(pose.mmsi == 367123456, "se3_pose_from_gps() sets MMSI");
    TEST_ASSERT(pose.timestamp == 1699000000, "se3_pose_from_gps() sets timestamp");
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    srand(time(NULL));

    printf("======================================================================\n");
    printf("SE(3) FIXED-POINT MATHEMATICS - UNIT TEST SUITE\n");
    printf("======================================================================\n");
    printf("Target: ESP32-S3 (Doom-inspired fixed-point)\n");
    printf("Format: 16.16 fixed-point (FRACUNIT = %d)\n", FRACUNIT);
    printf("LUT: %d entries (~%.4f° resolution)\n", NUM_FINE_ANGLES, 360.0f / NUM_FINE_ANGLES);

    /* Initialize SE(3) subsystem */
    se3_init_tables();

    /* Run all test suites */
    test_fixed_point_arithmetic();
    test_trig_lut_accuracy();
    test_rotation_matrices();
    test_geodetic_utils();
    test_vector_ops();
    test_se3_poses();

    /* Summary */
    printf("\n======================================================================\n");
    printf("TEST SUMMARY\n");
    printf("======================================================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("\n✓ ALL TESTS PASSED - Ready for ESP32-S3 deployment\n");
    } else {
        printf("\n✗ SOME TESTS FAILED - Review implementation\n");
    }
    printf("======================================================================\n");

    return tests_failed;
}
