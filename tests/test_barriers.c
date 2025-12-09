/**
 * test_barriers.c - Genesis v3.0 Barrier Potential Unit Tests
 *
 * Tests the barrier potential implementation from include/barriers.h.
 * Validates:
 *   - Barrier gradient behavior near constraint bounds
 *   - Gradient magnitude decreases away from bounds
 *   - Both lower and upper bound handling
 *   - Q16.16 fixed-point arithmetic correctness
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Include the barriers header */
#include "../include/barriers.h"

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/* ========================================================================
 * TEST HELPERS
 * ======================================================================== */

#define TEST_ASSERT(condition, message) do { \
    if (condition) { \
        tests_passed++; \
        printf("  PASS: %s\n", message); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", message); \
    } \
} while(0)

#define TEST_ASSERT_NEAR(actual, expected, tolerance, message) do { \
    float _diff = fabsf((float)(actual) - (float)(expected)); \
    if (_diff <= (tolerance)) { \
        tests_passed++; \
        printf("  PASS: %s (actual=%.6f, expected=%.6f, diff=%.6f)\n", \
               message, (float)(actual), (float)(expected), _diff); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (actual=%.6f, expected=%.6f, diff=%.6f > tol=%.6f)\n", \
               message, (float)(actual), (float)(expected), _diff, (float)(tolerance)); \
    } \
} while(0)

/* ========================================================================
 * TEST CASES
 * ======================================================================== */

/**
 * Test 1: Barrier gradient is negative when approaching lower bound.
 */
void test_barrier_gradient_negative_near_lower_bound(void) {
    printf("\n[Test 1] Barrier gradient negative near lower bound\n");

    fixed_t x_min = float_to_barrier_fixed(1.0f);
    fixed_t x_near = float_to_barrier_fixed(1.001f);  /* Just above x_min */

    fixed_t grad = fixed_barrier_gradient(x_near, x_min);

    /* Gradient should be negative (repulsive force away from bound) */
    TEST_ASSERT(grad < 0, "Gradient is negative near lower bound");

    /* Gradient should be large in magnitude */
    float grad_f = barrier_fixed_to_float(grad);
    TEST_ASSERT(fabsf(grad_f) > 100.0f, "Gradient magnitude is significant");
}

/**
 * Test 2: Barrier gradient magnitude decreases with distance from bound.
 */
void test_barrier_gradient_decreases_with_distance(void) {
    printf("\n[Test 2] Barrier gradient decreases with distance\n");

    fixed_t x_min = float_to_barrier_fixed(1.0f);
    fixed_t x_near = float_to_barrier_fixed(1.001f);
    fixed_t x_mid = float_to_barrier_fixed(1.1f);
    fixed_t x_far = float_to_barrier_fixed(2.0f);

    fixed_t grad_near = fixed_barrier_gradient(x_near, x_min);
    fixed_t grad_mid = fixed_barrier_gradient(x_mid, x_min);
    fixed_t grad_far = fixed_barrier_gradient(x_far, x_min);

    /* Use absolute values for comparison */
    fixed_t abs_near = barrier_fixed_abs(grad_near);
    fixed_t abs_mid = barrier_fixed_abs(grad_mid);
    fixed_t abs_far = barrier_fixed_abs(grad_far);

    TEST_ASSERT(abs_near > abs_mid, "|grad_near| > |grad_mid|");
    TEST_ASSERT(abs_mid > abs_far, "|grad_mid| > |grad_far|");
}

/**
 * Test 3: Upper bound barrier gradient is positive (pushes down).
 */
void test_barrier_gradient_upper_bound(void) {
    printf("\n[Test 3] Upper bound barrier gradient\n");

    fixed_t x_max = float_to_barrier_fixed(1.0f);
    fixed_t x_near = float_to_barrier_fixed(0.999f);  /* Just below x_max */

    fixed_t grad = fixed_barrier_gradient_upper(x_near, x_max);

    /* Gradient should be positive (force pushing down away from upper bound) */
    TEST_ASSERT(grad > 0, "Upper bound gradient is positive");
}

/**
 * Test 4: Bounded gradient combines lower and upper correctly.
 */
void test_barrier_gradient_bounded(void) {
    printf("\n[Test 4] Combined bounded gradient\n");

    fixed_t x_min = float_to_barrier_fixed(0.0f);
    fixed_t x_max = float_to_barrier_fixed(1.0f);

    /* Test at midpoint - both gradients should be small and roughly cancel */
    fixed_t x_mid = float_to_barrier_fixed(0.5f);
    fixed_t grad_mid = fixed_barrier_gradient_bounded(x_mid, x_min, x_max);
    float grad_mid_f = barrier_fixed_to_float(grad_mid);

    /* At midpoint, combined gradient should be small */
    TEST_ASSERT(fabsf(grad_mid_f) < 100.0f, "Combined gradient small at midpoint");

    /* Test near lower bound - should be strongly negative */
    fixed_t x_low = float_to_barrier_fixed(0.01f);
    fixed_t grad_low = fixed_barrier_gradient_bounded(x_low, x_min, x_max);

    TEST_ASSERT(grad_low < 0, "Combined gradient negative near lower bound");

    /* Test near upper bound - should be strongly positive */
    fixed_t x_high = float_to_barrier_fixed(0.99f);
    fixed_t grad_high = fixed_barrier_gradient_bounded(x_high, x_min, x_max);

    TEST_ASSERT(grad_high > 0, "Combined gradient positive near upper bound");
}

/**
 * Test 5: Barrier potential is always positive.
 */
void test_barrier_potential_positive(void) {
    printf("\n[Test 5] Barrier potential is positive\n");

    fixed_t x_min = float_to_barrier_fixed(0.0f);

    /* Test at various distances */
    float test_values[] = {0.001f, 0.01f, 0.1f, 0.5f, 1.0f, 10.0f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        fixed_t x = float_to_barrier_fixed(test_values[i]);
        fixed_t potential = fixed_barrier_potential(x, x_min);

        char msg[100];
        snprintf(msg, sizeof(msg), "Potential positive at x=%.3f", test_values[i]);
        TEST_ASSERT(potential > 0, msg);
    }
}

/**
 * Test 6: Deep violation returns maximum gradient.
 */
void test_barrier_deep_violation(void) {
    printf("\n[Test 6] Deep violation handling\n");

    fixed_t x_min = float_to_barrier_fixed(1.0f);
    fixed_t x_violation = float_to_barrier_fixed(0.5f);  /* Below x_min */

    fixed_t grad = fixed_barrier_gradient(x_violation, x_min);

    /* Should return maximum negative gradient */
    TEST_ASSERT(grad == -(fixed_t)0x7FFFFFFF, "Deep violation returns max gradient");
}

/**
 * Test 7: Fixed-point conversion round-trip accuracy.
 */
void test_fixed_point_conversion(void) {
    printf("\n[Test 7] Fixed-point conversion accuracy\n");

    float test_values[] = {0.0f, 0.5f, 1.0f, 10.0f, 100.0f, -1.0f, -10.0f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        float original = test_values[i];
        fixed_t fixed = float_to_barrier_fixed(original);
        float recovered = barrier_fixed_to_float(fixed);

        char msg[100];
        snprintf(msg, sizeof(msg), "Round-trip for %.2f", original);
        TEST_ASSERT_NEAR(recovered, original, 0.0001f, msg);
    }
}

/**
 * Test 8: Gradient is finite for valid inputs.
 */
void test_gradient_finite(void) {
    printf("\n[Test 8] Gradient is finite for valid inputs\n");

    fixed_t x_min = float_to_barrier_fixed(0.0f);

    /* Test that gradient doesn't overflow for reasonable inputs */
    fixed_t x = float_to_barrier_fixed(0.5f);
    fixed_t grad = fixed_barrier_gradient(x, x_min);

    float grad_f = barrier_fixed_to_float(grad);
    TEST_ASSERT(!isinf(grad_f) && !isnan(grad_f), "Gradient is finite");
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(void) {
    printf("======================================================================\n");
    printf("GENESIS v3.0 BARRIER POTENTIAL UNIT TESTS\n");
    printf("======================================================================\n");

    /* Run all tests */
    test_barrier_gradient_negative_near_lower_bound();
    test_barrier_gradient_decreases_with_distance();
    test_barrier_gradient_upper_bound();
    test_barrier_gradient_bounded();
    test_barrier_potential_positive();
    test_barrier_deep_violation();
    test_fixed_point_conversion();
    test_gradient_finite();

    /* Summary */
    printf("\n======================================================================\n");
    printf("SUMMARY: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("======================================================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
