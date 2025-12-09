/**
 * test_randomization.c - Genesis v3.0 Domain Randomization Unit Tests
 *
 * Tests the CLT-based Gaussian sampling implementation.
 * Validates:
 *   - Mean of samples approximates specified mean
 *   - Standard deviation of samples approximates specified std_dev
 *   - RNG determinism with same seed
 *   - Distribution is approximately Gaussian
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* ========================================================================
 * REPRODUCE PARAMETER_LOADER.C FUNCTIONS FOR TESTING
 * ======================================================================== */

#ifndef FRACUNIT
#define FRACUNIT 65536
#endif

typedef int32_t fixed_t;

static uint32_t rng_state = 0x12345678;

void param_rng_init(uint32_t seed) {
    rng_state = seed;
    if (rng_state == 0) {
        rng_state = 0x12345678;
    }
}

static inline uint32_t param_rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static inline float param_rng_uniform_f(void) {
    return (float)(param_rng_next() >> 8) / (float)(1u << 24);
}

static inline fixed_t param_rng_uniform_fixed(void) {
    return (fixed_t)(param_rng_next() >> 16);
}

float sample_gaussian_f(float mean, float std_dev) {
    float sum = 0.0f;
    for (int i = 0; i < 12; i++) {
        sum += param_rng_uniform_f();
    }
    float z = sum - 6.0f;
    return mean + std_dev * z;
}

fixed_t sample_gaussian_fixed(fixed_t mean, fixed_t std_dev) {
    int64_t sum = 0;
    for (int i = 0; i < 12; i++) {
        sum += param_rng_uniform_fixed();
    }
    fixed_t z_fixed = (fixed_t)(sum - (6L * (FRACUNIT / 2)));
    int64_t scaled = ((int64_t)z_fixed * (int64_t)std_dev) >> 16;
    return mean + (fixed_t)scaled;
}

/* Helper: convert float to fixed */
static inline fixed_t float_to_fixed(float f) {
    return (fixed_t)(f * (float)FRACUNIT + (f >= 0.0f ? 0.5f : -0.5f));
}

/* Helper: convert fixed to float */
static inline float fixed_to_float(fixed_t fx) {
    return (float)fx / (float)FRACUNIT;
}

/* ========================================================================
 * TEST FRAMEWORK
 * ======================================================================== */

static int tests_passed = 0;
static int tests_failed = 0;

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
        printf("  PASS: %s (actual=%.4f, expected=%.4f, diff=%.4f)\n", \
               message, (float)(actual), (float)(expected), _diff); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (actual=%.4f, expected=%.4f, diff=%.4f > tol=%.4f)\n", \
               message, (float)(actual), (float)(expected), _diff, (float)(tolerance)); \
    } \
} while(0)

/* ========================================================================
 * STATISTICAL HELPERS
 * ======================================================================== */

void compute_stats(const float* values, int n, float* mean_out, float* std_out) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += values[i];
    }
    double mean = sum / n;

    double sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = values[i] - mean;
        sum_sq += diff * diff;
    }
    double variance = sum_sq / (n - 1);

    *mean_out = (float)mean;
    *std_out = (float)sqrt(variance);
}

/* ========================================================================
 * TEST CASES
 * ======================================================================== */

/**
 * Test 1: Float Gaussian mean is approximately correct.
 */
void test_gaussian_float_mean(void) {
    printf("\n[Test 1] Float Gaussian mean accuracy\n");

    param_rng_init(12345);

    const float target_mean = 100.0f;
    const float target_std = 10.0f;
    const int N = 10000;

    float* samples = (float*)malloc(N * sizeof(float));
    for (int i = 0; i < N; i++) {
        samples[i] = sample_gaussian_f(target_mean, target_std);
    }

    float actual_mean, actual_std;
    compute_stats(samples, N, &actual_mean, &actual_std);

    /* Mean should be within 1% of target */
    float tolerance = target_mean * 0.01f;
    TEST_ASSERT_NEAR(actual_mean, target_mean, tolerance, "Float Gaussian mean");

    free(samples);
}

/**
 * Test 2: Float Gaussian std_dev is approximately correct.
 */
void test_gaussian_float_stddev(void) {
    printf("\n[Test 2] Float Gaussian std_dev accuracy\n");

    param_rng_init(12345);

    const float target_mean = 100.0f;
    const float target_std = 10.0f;
    const int N = 10000;

    float* samples = (float*)malloc(N * sizeof(float));
    for (int i = 0; i < N; i++) {
        samples[i] = sample_gaussian_f(target_mean, target_std);
    }

    float actual_mean, actual_std;
    compute_stats(samples, N, &actual_mean, &actual_std);

    /* Std dev should be within 10% of target (CLT approximation is not perfect) */
    float tolerance = target_std * 0.10f;
    TEST_ASSERT_NEAR(actual_std, target_std, tolerance, "Float Gaussian std_dev");

    free(samples);
}

/**
 * Test 3: Fixed-point Gaussian mean accuracy.
 */
void test_gaussian_fixed_mean(void) {
    printf("\n[Test 3] Fixed-point Gaussian mean accuracy\n");

    param_rng_init(54321);

    const float target_mean_f = 100.0f;
    const float target_std_f = 2.0f;
    const fixed_t target_mean = float_to_fixed(target_mean_f);
    const fixed_t target_std = float_to_fixed(target_std_f);
    const int N = 1000;

    float* samples = (float*)malloc(N * sizeof(float));
    for (int i = 0; i < N; i++) {
        fixed_t sample = sample_gaussian_fixed(target_mean, target_std);
        samples[i] = fixed_to_float(sample);
    }

    float actual_mean, actual_std;
    compute_stats(samples, N, &actual_mean, &actual_std);

    /* Mean should be within 0.5 units in fixed-point space */
    TEST_ASSERT_NEAR(actual_mean, target_mean_f, 0.5f, "Fixed-point Gaussian mean");

    free(samples);
}

/**
 * Test 4: RNG determinism - same seed produces same sequence.
 */
void test_rng_determinism(void) {
    printf("\n[Test 4] RNG determinism\n");

    /* First sequence */
    param_rng_init(99999);
    float seq1[10];
    for (int i = 0; i < 10; i++) {
        seq1[i] = sample_gaussian_f(0.0f, 1.0f);
    }

    /* Reset and generate again */
    param_rng_init(99999);
    float seq2[10];
    for (int i = 0; i < 10; i++) {
        seq2[i] = sample_gaussian_f(0.0f, 1.0f);
    }

    /* Sequences should be identical */
    int all_match = 1;
    for (int i = 0; i < 10; i++) {
        if (seq1[i] != seq2[i]) {
            all_match = 0;
            break;
        }
    }

    TEST_ASSERT(all_match, "Same seed produces identical sequence");
}

/**
 * Test 5: Different seeds produce different sequences.
 */
void test_rng_different_seeds(void) {
    printf("\n[Test 5] Different seeds produce different sequences\n");

    param_rng_init(11111);
    float val1 = sample_gaussian_f(0.0f, 1.0f);

    param_rng_init(22222);
    float val2 = sample_gaussian_f(0.0f, 1.0f);

    TEST_ASSERT(val1 != val2, "Different seeds produce different values");
}

/**
 * Test 6: Zero std_dev returns constant mean.
 */
void test_zero_stddev(void) {
    printf("\n[Test 6] Zero std_dev returns mean\n");

    param_rng_init(12345);

    const float mean = 42.0f;
    float sample = sample_gaussian_f(mean, 0.0f);

    TEST_ASSERT_NEAR(sample, mean, 0.001f, "Zero std_dev returns exact mean");
}

/**
 * Test 7: Distribution is approximately symmetric around mean.
 */
void test_distribution_symmetry(void) {
    printf("\n[Test 7] Distribution symmetry\n");

    param_rng_init(77777);

    const float mean = 50.0f;
    const float std = 5.0f;
    const int N = 5000;

    int above = 0;
    int below = 0;

    for (int i = 0; i < N; i++) {
        float sample = sample_gaussian_f(mean, std);
        if (sample > mean) above++;
        else if (sample < mean) below++;
    }

    /* Should be roughly 50/50 (within 5% tolerance) */
    float ratio = (float)above / (float)(above + below);
    TEST_ASSERT(ratio > 0.45f && ratio < 0.55f, "Distribution is symmetric around mean");
}

/**
 * Test 8: Uniform distribution covers full range.
 */
void test_uniform_range(void) {
    printf("\n[Test 8] Uniform distribution range\n");

    param_rng_init(88888);

    float min_val = 1.0f;
    float max_val = 0.0f;

    for (int i = 0; i < 10000; i++) {
        float u = param_rng_uniform_f();
        if (u < min_val) min_val = u;
        if (u > max_val) max_val = u;
    }

    TEST_ASSERT(min_val < 0.01f, "Uniform samples reach near 0");
    TEST_ASSERT(max_val > 0.99f, "Uniform samples reach near 1");
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(void) {
    printf("======================================================================\n");
    printf("GENESIS v3.0 DOMAIN RANDOMIZATION UNIT TESTS\n");
    printf("======================================================================\n");

    /* Run all tests */
    test_gaussian_float_mean();
    test_gaussian_float_stddev();
    test_gaussian_fixed_mean();
    test_rng_determinism();
    test_rng_different_seeds();
    test_zero_stddev();
    test_distribution_symmetry();
    test_uniform_range();

    /* Summary */
    printf("\n======================================================================\n");
    printf("SUMMARY: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("======================================================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
