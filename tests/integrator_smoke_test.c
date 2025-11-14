/*
 * integrator_smoke_test.c - Pre-Flight Integrator Sanity Check
 *
 * Fast smoke test to catch obvious bugs before running full benchmark.
 * Runs each integrator for 10 steps on a pure rotation.
 *
 * Pass criteria:
 *   - No crashes or numerical exceptions
 *   - SO(3) orthogonality preserved (det(R) = 1 within 1e-6)
 *   - Rotation norm within bounds
 *
 * This should be part of standard `make test` build.
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../embedded/se3_edge.h"

/* ========================================================================
 * TEST CONFIGURATION
 * ======================================================================== */

#define NUM_STEPS 10
#define EPSILON 1e-6

/* ========================================================================
 * INTEGRATOR STUBS (To be implemented)
 * ======================================================================== */

typedef enum {
    INTEGRATOR_LIE_EULER,
    INTEGRATOR_RKMK,
    INTEGRATOR_CROUCH_GROSSMAN
} IntegratorType;

/* Stub integrators - these will be implemented later */
void lie_euler_step(se3_pose_t* pose, float dt) {
    /* TODO: Implement Lie-Euler integrator */
    /* For now, just apply a small rotation */
    uint32_t angle = (uint32_t)(0.1 * 0xFFFFFFFF / 360.0);  /* 0.1 degree */
    fixed_t R_new[9];
    rotation_from_yaw(angle, R_new);
    rotation_mul(pose->rotation, R_new, pose->rotation);
}

void rkmk_step(se3_pose_t* pose, float dt) {
    /* TODO: Implement RKMK integrator */
    /* For now, same as Lie-Euler */
    lie_euler_step(pose, dt);
}

void crouch_grossman_step(se3_pose_t* pose, float dt) {
    /* TODO: Implement Crouch-Grossman integrator */
    /* For now, same as Lie-Euler */
    lie_euler_step(pose, dt);
}

/* ========================================================================
 * MATRIX UTILITIES
 * ======================================================================== */

/* Compute determinant of 3x3 matrix */
double mat3_det(const fixed_t R[9]) {
    /* Convert to double for precision */
    double r[9];
    for (int i = 0; i < 9; i++) {
        r[i] = FIXED_TO_FLOAT(R[i]);
    }

    /* det(R) = r00(r11*r22 - r12*r21) - r01(r10*r22 - r12*r20) + r02(r10*r21 - r11*r20) */
    double det = r[0] * (r[4] * r[8] - r[5] * r[7])
               - r[1] * (r[3] * r[8] - r[5] * r[6])
               + r[2] * (r[3] * r[7] - r[4] * r[6]);

    return det;
}

/* Check if matrix is orthogonal (R^T * R = I) */
bool is_orthogonal(const fixed_t R[9], double tolerance) {
    /* Compute R^T * R */
    fixed_t RTR[9];

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int64_t sum = 0;
            for (int k = 0; k < 3; k++) {
                /* R^T[i,k] = R[k,i] */
                sum += (int64_t)R[k*3 + i] * (int64_t)R[k*3 + j];
            }
            RTR[i*3 + j] = (fixed_t)(sum >> FRACBITS);
        }
    }

    /* Check if RTR is identity */
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            fixed_t expected = (i == j) ? FRACUNIT : 0;
            fixed_t diff = RTR[i*3 + j] - expected;
            double error = fabs(FIXED_TO_FLOAT(diff));
            if (error > tolerance) {
                return false;
            }
        }
    }

    return true;
}

/* ========================================================================
 * SMOKE TEST
 * ======================================================================== */

typedef struct {
    const char* name;
    void (*step_func)(se3_pose_t*, float);
} IntegratorTest;

bool run_smoke_test(IntegratorTest* test) {
    printf("Testing %s integrator...\n", test->name);

    /* Initialize pose to identity */
    se3_pose_t pose;
    se3_pose_identity(&pose);

    float dt = 0.01f;  /* 10ms timestep */

    /* Run for NUM_STEPS */
    for (int i = 0; i < NUM_STEPS; i++) {
        test->step_func(&pose, dt);

        /* Check for NaN/Inf */
        for (int j = 0; j < 9; j++) {
            if (!isfinite(FIXED_TO_FLOAT(pose.rotation[j]))) {
                printf("  FAIL: Non-finite value in rotation matrix at step %d\n", i);
                return false;
            }
        }

        /* Check determinant */
        double det = mat3_det(pose.rotation);
        double det_error = fabs(det - 1.0);
        if (det_error > EPSILON) {
            printf("  FAIL: Determinant error %.2e at step %d (expected 1.0, got %.6f)\n",
                   det_error, i, det);
            return false;
        }

        /* Check orthogonality */
        if (!is_orthogonal(pose.rotation, EPSILON)) {
            printf("  FAIL: Matrix not orthogonal at step %d\n", i);
            return false;
        }
    }

    printf("  PASS: All checks passed\n");
    return true;
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(void) {
    printf("=================================================================\n");
    printf("INTEGRATOR SMOKE TEST - Pre-Flight Sanity Check\n");
    printf("=================================================================\n\n");

    IntegratorTest tests[] = {
        {"Lie-Euler", lie_euler_step},
        {"RKMK", rkmk_step},
        {"Crouch-Grossman", crouch_grossman_step}
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    for (int i = 0; i < num_tests; i++) {
        if (run_smoke_test(&tests[i])) {
            passed++;
        }
        printf("\n");
    }

    printf("=================================================================\n");
    printf("Results: %d/%d tests passed\n", passed, num_tests);
    printf("=================================================================\n");

    if (passed == num_tests) {
        printf("✓ ALL SMOKE TESTS PASSED - Safe to proceed to full benchmark\n");
        return 0;
    } else {
        printf("✗ SOME TESTS FAILED - Fix before running full benchmark\n");
        return 1;
    }
}
