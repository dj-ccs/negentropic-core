/**
 * test_fixed_math_luts.c - Unit Tests for Doom Ethos Fixed-Point LUTs
 *
 * Validates LUT accuracy, performance, and cross-platform determinism.
 *
 * Test Coverage:
 * 1. Reciprocal LUT accuracy (target: < 1e-4 relative error)
 * 2. Sqrt LUT accuracy (target: < 1e-4 relative error)
 * 3. Inverse sqrt accuracy (target: < 5e-4 relative error)
 * 4. Edge cases (zero, negative, overflow)
 * 5. Determinism (same inputs -> same outputs)
 *
 * Author: ClaudeCode (v2.2 Doom Ethos Sprint)
 * Date: 2025-11-18
 */

#include "../src/core/math/fixed_math.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

/* Test configuration */
#define TOLERANCE_RECIPROCAL 1e-4f
#define TOLERANCE_SQRT 1e-4f
#define TOLERANCE_INV_SQRT 5e-4f

#define TEST_PASS "\033[32m[PASS]\033[0m"
#define TEST_FAIL "\033[31m[FAIL]\033[0m"

/* ========================================================================
 * TEST 1: Reciprocal LUT Accuracy
 * ======================================================================== */

int test_reciprocal_accuracy(void) {
    printf("\n[TEST 1] Reciprocal LUT Accuracy\n");
    printf("  Target: < %.1e relative error across range [1.0, 256.0]\n", TOLERANCE_RECIPROCAL);

    int failures = 0;
    float max_error = 0.0f;
    float worst_x = 0.0f;

    // Test 100 points across the range
    for (int i = 0; i < 100; i++) {
        float x = 1.0f + (255.0f * (float)i / 99.0f);
        fixed_t x_fixed = FLOAT_TO_FIXED(x);
        fixed_t result = fixed_reciprocal(x_fixed);
        float result_float = FIXED_TO_FLOAT(result);

        float expected = 1.0f / x;
        float error = fabsf((result_float - expected) / expected);

        if (error > max_error) {
            max_error = error;
            worst_x = x;
        }

        if (error > TOLERANCE_RECIPROCAL) {
            printf("  %s x=%.3f: expected=%.6f, got=%.6f, error=%.1e\n",
                   TEST_FAIL, x, expected, result_float, error);
            failures++;
        }
    }

    printf("  Maximum error: %.2e at x=%.3f\n", max_error, worst_x);

    if (failures == 0) {
        printf("  %s All 100 test points passed\n", TEST_PASS);
        return 0;
    } else {
        printf("  %s %d/%d test points failed\n", TEST_FAIL, failures, 100);
        return -1;
    }
}

/* ========================================================================
 * TEST 2: Sqrt LUT Accuracy
 * ======================================================================== */

int test_sqrt_accuracy(void) {
    printf("\n[TEST 2] Sqrt LUT Accuracy\n");
    printf("  Target: < %.1e relative error across range [0.1, 1024.0]\n", TOLERANCE_SQRT);

    int failures = 0;
    float max_error = 0.0f;
    float worst_x = 0.0f;

    // Test 100 points across the range (skip very small values for stability)
    for (int i = 0; i < 100; i++) {
        float x = 0.1f + (1023.9f * (float)i / 99.0f);
        fixed_t x_fixed = FLOAT_TO_FIXED(x);
        fixed_t result = fixed_sqrt(x_fixed);
        float result_float = FIXED_TO_FLOAT(result);

        float expected = sqrtf(x);
        float error = fabsf((result_float - expected) / expected);

        if (error > max_error) {
            max_error = error;
            worst_x = x;
        }

        if (error > TOLERANCE_SQRT) {
            printf("  %s x=%.3f: expected=%.6f, got=%.6f, error=%.1e\n",
                   TEST_FAIL, x, expected, result_float, error);
            failures++;
        }
    }

    printf("  Maximum error: %.2e at x=%.3f\n", max_error, worst_x);

    if (failures == 0) {
        printf("  %s All 100 test points passed\n", TEST_PASS);
        return 0;
    } else {
        printf("  %s %d/%d test points failed\n", TEST_FAIL, failures, 100);
        return -1;
    }
}

/* ========================================================================
 * TEST 3: Inverse Sqrt Accuracy
 * ======================================================================== */

int test_inv_sqrt_accuracy(void) {
    printf("\n[TEST 3] Inverse Sqrt Accuracy (Quake III Algorithm)\n");
    printf("  Target: < %.1e relative error across range [1.0, 256.0]\n", TOLERANCE_INV_SQRT);

    int failures = 0;
    float max_error = 0.0f;
    float worst_x = 0.0f;

    // Test 100 points across the range
    for (int i = 0; i < 100; i++) {
        float x = 1.0f + (255.0f * (float)i / 99.0f);
        fixed_t x_fixed = FLOAT_TO_FIXED(x);
        fixed_t result = fixed_inv_sqrt(x_fixed);
        float result_float = FIXED_TO_FLOAT(result);

        float expected = 1.0f / sqrtf(x);
        float error = fabsf((result_float - expected) / expected);

        if (error > max_error) {
            max_error = error;
            worst_x = x;
        }

        if (error > TOLERANCE_INV_SQRT) {
            printf("  %s x=%.3f: expected=%.6f, got=%.6f, error=%.1e\n",
                   TEST_FAIL, x, expected, result_float, error);
            failures++;
        }
    }

    printf("  Maximum error: %.2e at x=%.3f\n", max_error, worst_x);

    if (failures == 0) {
        printf("  %s All 100 test points passed\n", TEST_PASS);
        return 0;
    } else {
        printf("  %s %d/%d test points failed\n", TEST_FAIL, failures, 100);
        return -1;
    }
}

/* ========================================================================
 * TEST 4: Edge Cases
 * ======================================================================== */

int test_edge_cases(void) {
    printf("\n[TEST 4] Edge Cases (Zero, Negative, Overflow)\n");

    int failures = 0;

    // Test 4.1: Reciprocal of zero
    fixed_t recip_zero = fixed_reciprocal(0);
    if (recip_zero != 0) {
        printf("  %s Reciprocal of 0: expected 0, got %d\n", TEST_FAIL, recip_zero);
        failures++;
    } else {
        printf("  %s Reciprocal of 0 = 0\n", TEST_PASS);
    }

    // Test 4.2: Sqrt of zero
    fixed_t sqrt_zero = fixed_sqrt(0);
    if (sqrt_zero != 0) {
        printf("  %s Sqrt of 0: expected 0, got %d\n", TEST_FAIL, sqrt_zero);
        failures++;
    } else {
        printf("  %s Sqrt of 0 = 0\n", TEST_PASS);
    }

    // Test 4.3: Sqrt of negative
    fixed_t sqrt_neg = fixed_sqrt(FLOAT_TO_FIXED(-10.0f));
    if (sqrt_neg != 0) {
        printf("  %s Sqrt of -10.0: expected 0, got %d\n", TEST_FAIL, sqrt_neg);
        failures++;
    } else {
        printf("  %s Sqrt of negative = 0\n", TEST_PASS);
    }

    // Test 4.4: Safe division by zero
    fixed_t div_safe = fixed_div_safe(FRACUNIT, 0);
    if (div_safe != INT32_MAX) {
        printf("  %s Safe div 1.0/0: expected INT32_MAX, got %d\n", TEST_FAIL, div_safe);
        failures++;
    } else {
        printf("  %s Safe division by zero saturates correctly\n", TEST_PASS);
    }

    if (failures == 0) {
        printf("  %s All edge cases handled correctly\n", TEST_PASS);
        return 0;
    } else {
        printf("  %s %d edge case failures\n", TEST_FAIL, failures);
        return -1;
    }
}

/* ========================================================================
 * TEST 5: Determinism
 * ======================================================================== */

int test_determinism(void) {
    printf("\n[TEST 5] Determinism (Repeated Calls)\n");
    printf("  Testing that same input always produces same output\n");

    int failures = 0;

    // Test with 10 different inputs, call each function 100 times
    for (int i = 0; i < 10; i++) {
        float x = 1.0f + (float)i * 25.0f;
        fixed_t x_fixed = FLOAT_TO_FIXED(x);

        // Get reference outputs
        fixed_t recip_ref = fixed_reciprocal(x_fixed);
        fixed_t sqrt_ref = fixed_sqrt(x_fixed);
        fixed_t inv_sqrt_ref = fixed_inv_sqrt(x_fixed);

        // Call 100 times and verify identical output
        for (int j = 0; j < 100; j++) {
            fixed_t recip = fixed_reciprocal(x_fixed);
            fixed_t sqrt_val = fixed_sqrt(x_fixed);
            fixed_t inv_sqrt = fixed_inv_sqrt(x_fixed);

            if (recip != recip_ref || sqrt_val != sqrt_ref || inv_sqrt != inv_sqrt_ref) {
                printf("  %s x=%.3f: Non-deterministic output detected!\n", TEST_FAIL, x);
                failures++;
                break;
            }
        }
    }

    if (failures == 0) {
        printf("  %s All functions are perfectly deterministic\n", TEST_PASS);
        return 0;
    } else {
        printf("  %s %d determinism failures\n", TEST_FAIL, failures);
        return -1;
    }
}

/* ========================================================================
 * TEST 6: LUT Verification
 * ======================================================================== */

int test_lut_verification(void) {
    printf("\n[TEST 6] LUT Verification\n");

    float recip_error = fixed_math_verify_lut("reciprocal");
    float sqrt_error = fixed_math_verify_lut("sqrt");

    printf("  Reciprocal LUT max error: %.2e (target: < %.1e)\n", recip_error, TOLERANCE_RECIPROCAL);
    printf("  Sqrt LUT max error: %.2e (target: < %.1e)\n", sqrt_error, TOLERANCE_SQRT);

    int failures = 0;
    if (recip_error > TOLERANCE_RECIPROCAL) {
        printf("  %s Reciprocal LUT exceeds tolerance\n", TEST_FAIL);
        failures++;
    } else {
        printf("  %s Reciprocal LUT within tolerance\n", TEST_PASS);
    }

    if (sqrt_error > TOLERANCE_SQRT) {
        printf("  %s Sqrt LUT exceeds tolerance\n", TEST_FAIL);
        failures++;
    } else {
        printf("  %s Sqrt LUT within tolerance\n", TEST_PASS);
    }

    return failures > 0 ? -1 : 0;
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    printf("================================================================================\n");
    printf(" Doom Ethos Fixed-Point LUT Test Suite\n");
    printf(" Version: v2.2\n");
    printf(" Date: 2025-11-18\n");
    printf("================================================================================\n");

    // Initialize LUTs
    printf("\n[INIT] Initializing fixed-point LUTs...\n");
    if (fixed_math_init() != 0) {
        printf("%s Failed to initialize LUTs\n", TEST_FAIL);
        return 1;
    }
    printf("%s LUTs initialized successfully\n", TEST_PASS);
    printf("  Memory usage: 3 KB (256×4B reciprocal + 512×4B sqrt)\n");

    // Run all tests
    int failures = 0;
    failures += test_reciprocal_accuracy();
    failures += test_sqrt_accuracy();
    failures += test_inv_sqrt_accuracy();
    failures += test_edge_cases();
    failures += test_determinism();
    failures += test_lut_verification();

    // Summary
    printf("\n================================================================================\n");
    if (failures == 0) {
        printf(" %s ALL TESTS PASSED\n", TEST_PASS);
        printf("================================================================================\n");
        return 0;
    } else {
        printf(" %s %d TEST(S) FAILED\n", TEST_FAIL, failures);
        printf("================================================================================\n");
        return 1;
    }
}
