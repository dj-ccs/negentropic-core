/*
 * rng.c - Deterministic Random Number Generator Implementation
 *
 * Implements xorshift64* algorithm for fast, deterministic RNG.
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#include "include/rng.h"

/* Default non-zero seed */
#define NEG_RNG_DEFAULT_SEED 0xDEADBEEFCAFEBABEULL

/* Multiplier for xorshift64* */
#define NEG_RNG_MULTIPLIER 0x2545F4914F6CDD1DULL

/* ========================================================================
 * RNG IMPLEMENTATION
 * ======================================================================== */

void neg_rng_seed(NegRNG* rng, uint64_t seed) {
    if (!rng) return;

    /* Ensure seed is non-zero */
    rng->state = (seed == 0) ? NEG_RNG_DEFAULT_SEED : seed;
}

uint64_t neg_rng_next(NegRNG* rng) {
    if (!rng) return 0;

    /* Ensure state is non-zero (should never happen if properly initialized) */
    if (rng->state == 0) {
        rng->state = NEG_RNG_DEFAULT_SEED;
    }

    /* xorshift64* algorithm */
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;

    return x * NEG_RNG_MULTIPLIER;
}

uint32_t neg_rng_next_u32(NegRNG* rng) {
    return (uint32_t)(neg_rng_next(rng) >> 32);
}

double neg_rng_next_double(NegRNG* rng) {
    /* Convert to [0.0, 1.0) using upper 53 bits */
    uint64_t x = neg_rng_next(rng) >> 11;  /* Use upper 53 bits */
    return (double)x / (double)(1ULL << 53);
}

float neg_rng_next_float(NegRNG* rng) {
    /* Convert to [0.0, 1.0) using upper 24 bits */
    uint32_t x = neg_rng_next_u32(rng) >> 8;  /* Use upper 24 bits */
    return (float)x / (float)(1U << 24);
}

int64_t neg_rng_range(NegRNG* rng, int64_t min, int64_t max) {
    if (min >= max) return min;

    uint64_t range = (uint64_t)(max - min + 1);
    uint64_t rand_val = neg_rng_next(rng);

    return min + (int64_t)(rand_val % range);
}
