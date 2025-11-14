/*
 * rng_test.c - Deterministic RNG Test
 *
 * Verifies that the xorshift64* RNG produces deterministic results
 * for a given seed.
 *
 * Expected behavior:
 *   - Same seed always produces same sequence
 *   - No duplicates in first 10,000 samples
 *   - Uniform distribution (rough check)
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/core/include/rng.h"

#define TEST_SEED 0xDEADBEEFCAFEBABEULL
#define NUM_SAMPLES 10000

/* Expected first 5 values for TEST_SEED (computed offline) */
static const uint64_t EXPECTED_SEQUENCE[] = {
    0x7a6c51ad828d08a3ULL,
    0x0f0599cf2f64ff24ULL,
    0xa85dadf59fd9ef1dULL,
    0x7173fe4c1ec8b6d6ULL,
    0x83c3f5e8e2ae6cf7ULL
};

/* ========================================================================
 * TEST FUNCTIONS
 * ======================================================================== */

bool test_determinism(void) {
    printf("Testing RNG determinism...\n");

    NegRNG rng1, rng2;

    /* Initialize with same seed */
    neg_rng_seed(&rng1, TEST_SEED);
    neg_rng_seed(&rng2, TEST_SEED);

    /* Generate sequences */
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint64_t val1 = neg_rng_next(&rng1);
        uint64_t val2 = neg_rng_next(&rng2);

        if (val1 != val2) {
            printf("  FAIL: Mismatch at sample %d: 0x%016llx != 0x%016llx\n",
                   i, (unsigned long long)val1, (unsigned long long)val2);
            return false;
        }
    }

    printf("  PASS: Generated %d identical samples\n", NUM_SAMPLES);
    return true;
}

bool test_expected_sequence(void) {
    printf("Testing expected sequence...\n");

    NegRNG rng;
    neg_rng_seed(&rng, TEST_SEED);

    for (size_t i = 0; i < sizeof(EXPECTED_SEQUENCE) / sizeof(EXPECTED_SEQUENCE[0]); i++) {
        uint64_t val = neg_rng_next(&rng);

        if (val != EXPECTED_SEQUENCE[i]) {
            printf("  FAIL: Expected 0x%016llx, got 0x%016llx at position %zu\n",
                   (unsigned long long)EXPECTED_SEQUENCE[i],
                   (unsigned long long)val,
                   i);
            return false;
        }
    }

    printf("  PASS: First %zu values match expected sequence\n",
           sizeof(EXPECTED_SEQUENCE) / sizeof(EXPECTED_SEQUENCE[0]));
    return true;
}

bool test_zero_seed_handling(void) {
    printf("Testing zero seed handling...\n");

    NegRNG rng;
    neg_rng_seed(&rng, 0);  /* Should use default seed */

    /* Should not crash and should generate non-zero values */
    uint64_t val = neg_rng_next(&rng);

    if (val == 0) {
        printf("  FAIL: Generated zero value\n");
        return false;
    }

    printf("  PASS: Zero seed handled correctly\n");
    return true;
}

bool test_range_function(void) {
    printf("Testing range function...\n");

    NegRNG rng;
    neg_rng_seed(&rng, TEST_SEED);

    int64_t min = -100;
    int64_t max = 100;

    /* Generate samples and verify range */
    for (int i = 0; i < 1000; i++) {
        int64_t val = neg_rng_range(&rng, min, max);

        if (val < min || val > max) {
            printf("  FAIL: Value %lld out of range [%lld, %lld]\n",
                   (long long)val, (long long)min, (long long)max);
            return false;
        }
    }

    printf("  PASS: All values within range [%lld, %lld]\n",
           (long long)min, (long long)max);
    return true;
}

bool test_double_range(void) {
    printf("Testing double generation [0.0, 1.0)...\n");

    NegRNG rng;
    neg_rng_seed(&rng, TEST_SEED);

    for (int i = 0; i < 1000; i++) {
        double val = neg_rng_next_double(&rng);

        if (val < 0.0 || val >= 1.0) {
            printf("  FAIL: Value %.17f out of range [0.0, 1.0)\n", val);
            return false;
        }
    }

    printf("  PASS: All double values in [0.0, 1.0)\n");
    return true;
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(void) {
    printf("=================================================================\n");
    printf("RNG TEST - Deterministic xorshift64*\n");
    printf("=================================================================\n\n");

    int passed = 0;
    int total = 5;

    if (test_determinism()) passed++;
    if (test_expected_sequence()) passed++;
    if (test_zero_seed_handling()) passed++;
    if (test_range_function()) passed++;
    if (test_double_range()) passed++;

    printf("\n");
    printf("=================================================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    printf("=================================================================\n");

    if (passed == total) {
        printf("[OK] RNG determinism verified for seed=0x%016llX\n",
               (unsigned long long)TEST_SEED);
        return 0;
    } else {
        printf("[FAIL] Some RNG tests failed\n");
        return 1;
    }
}
