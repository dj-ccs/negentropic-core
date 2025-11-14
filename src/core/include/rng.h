/*
 * rng.h - Deterministic Random Number Generator
 *
 * Implements xorshift64* for fast, deterministic random number generation.
 * Used for tests and future stochastic processes.
 *
 * DO NOT use stdlib.h rand() anywhere in negentropic-core.
 *
 * Reference: Vigna, S. (2016). "An experimental exploration of Marsaglia's
 *            xorshift generators, scrambled." arXiv:1402.6246
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#ifndef NEG_RNG_H
#define NEG_RNG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * RNG STATE
 * ======================================================================== */

/**
 * RNG state (64-bit seed).
 *
 * Must never be zero. If initialized to zero, it will be reset to a
 * default non-zero seed.
 */
typedef struct {
    uint64_t state;  /* Current RNG state (must be non-zero) */
} NegRNG;

/* ========================================================================
 * RNG FUNCTIONS
 * ======================================================================== */

/**
 * Initialize RNG with a seed.
 *
 * If seed is 0, a default non-zero seed will be used.
 *
 * @param rng RNG state
 * @param seed Initial seed (non-zero)
 */
void neg_rng_seed(NegRNG* rng, uint64_t seed);

/**
 * Generate next random 64-bit unsigned integer.
 *
 * Uses xorshift64* algorithm for fast, deterministic generation.
 *
 * @param rng RNG state
 * @return Random 64-bit unsigned integer
 */
uint64_t neg_rng_next(NegRNG* rng);

/**
 * Generate random 32-bit unsigned integer.
 *
 * @param rng RNG state
 * @return Random 32-bit unsigned integer
 */
uint32_t neg_rng_next_u32(NegRNG* rng);

/**
 * Generate random double in range [0.0, 1.0).
 *
 * @param rng RNG state
 * @return Random double in [0.0, 1.0)
 */
double neg_rng_next_double(NegRNG* rng);

/**
 * Generate random float in range [0.0, 1.0).
 *
 * @param rng RNG state
 * @return Random float in [0.0, 1.0)
 */
float neg_rng_next_float(NegRNG* rng);

/**
 * Generate random integer in range [min, max].
 *
 * @param rng RNG state
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return Random integer in [min, max]
 */
int64_t neg_rng_range(NegRNG* rng, int64_t min, int64_t max);

#ifdef __cplusplus
}
#endif

#endif /* NEG_RNG_H */
