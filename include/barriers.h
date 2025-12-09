/**
 * barriers.h - Genesis v3.0 Unbreakable Solver Barrier Potentials
 *
 * Implements smooth, strictly convex, C^1 barrier potentials that replace
 * discrete clamps and if-statements throughout the physics solvers.
 *
 * CANONICAL PRINCIPLE #4: Constraints are Energy
 *   - Only smooth, strictly convex, C^1 barrier potentials
 *   - No clamps, no if-statements for physical bounds
 *   - Thermodynamic consistency through energetic penalties
 *
 * Mathematical Foundation:
 *   barrier(x, x_min) = kappa * -log(x - x_min + epsilon)
 *
 *   Since exact logarithm is expensive in fixed-point, we use a convex
 *   surrogate: 1/(x - x_min + epsilon) which has similar behavior:
 *   - Large positive values near the bound (strong penalty)
 *   - Rapidly decaying away from the bound
 *   - Continuous and differentiable (C^1)
 *   - Strictly convex
 *
 * All arithmetic is Q16.16 fixed-point per Doom Ethos standard.
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

#ifndef INCLUDE_BARRIERS_H
#define INCLUDE_BARRIERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Q16.16 FIXED-POINT DEFINITIONS
 * ======================================================================== */

/**
 * Q16.16 fixed-point format (Doom Ethos)
 * Range: -32768.0 to +32767.99998474121
 * Precision: 1.52587890625e-05 (1/65536)
 */
#ifndef FRACUNIT
#define FRACUNIT 65536
#endif

#ifndef FIXED_SHIFT
#define FIXED_SHIFT 16
#endif

typedef int32_t fixed_t;

/* ========================================================================
 * BARRIER PARAMETERS (Genesis v3.0 Canonical Values)
 * ======================================================================== */

/**
 * BARRIER_STRENGTH: Base strength coefficient (kappa)
 * Value: 8.0 in Q16.16 = 0x00080000
 *
 * This controls the magnitude of the energetic penalty as state
 * approaches the constraint boundary. Higher values create steeper
 * penalties but may require smaller timesteps.
 *
 * Callers may scale this by application-specific factors.
 */
#define BARRIER_STRENGTH 0x00080000  /* 8.0 in Q16.16 */

/**
 * BARRIER_EPS: Small epsilon to prevent singularity
 * Value: ~1e-9 in Q16.16 = 0x00000040 (64/65536 ~ 0.001)
 *
 * Note: Due to Q16.16 precision limits, the smallest representable
 * value is ~1.5e-5. We use a slightly larger epsilon for stability.
 */
#define BARRIER_EPS 0x00000040  /* ~0.001 in Q16.16 */

/* ========================================================================
 * HELPER MACROS FOR FIXED-POINT ARITHMETIC
 * ======================================================================== */

/**
 * Fixed-point multiplication: (a * b) >> 16
 * Uses 64-bit intermediate to prevent overflow
 */
static inline fixed_t barrier_fixed_mul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) >> FIXED_SHIFT);
}

/**
 * Fixed-point division: (a << 16) / b
 * Uses 64-bit intermediate to prevent overflow
 * Returns 0 if b is 0 (safe division)
 */
static inline fixed_t barrier_fixed_div(fixed_t a, fixed_t b) {
    if (b == 0) return 0;
    return (fixed_t)((((int64_t)a) << FIXED_SHIFT) / b);
}

/**
 * Fixed-point subtraction with saturation
 */
static inline fixed_t barrier_fixed_sub(fixed_t a, fixed_t b) {
    int64_t result = (int64_t)a - (int64_t)b;
    if (result > INT32_MAX) return INT32_MAX;
    if (result < INT32_MIN) return INT32_MIN;
    return (fixed_t)result;
}

/**
 * Fixed-point addition with saturation
 */
static inline fixed_t barrier_fixed_add(fixed_t a, fixed_t b) {
    int64_t result = (int64_t)a + (int64_t)b;
    if (result > INT32_MAX) return INT32_MAX;
    if (result < INT32_MIN) return INT32_MIN;
    return (fixed_t)result;
}

/**
 * Absolute value of fixed-point number
 */
static inline fixed_t barrier_fixed_abs(fixed_t x) {
    return (x < 0) ? -x : x;
}

/* ========================================================================
 * BARRIER POTENTIAL FUNCTIONS
 * ======================================================================== */

/**
 * Compute barrier potential value (energy contribution).
 *
 * U_barrier(x) = kappa / (x - x_min + epsilon)
 *
 * This is a convex surrogate for kappa * -log(x - x_min + epsilon).
 * It provides:
 *   - Large positive values as x approaches x_min (strong penalty)
 *   - Rapidly decaying values away from x_min
 *   - Strictly convex, C^1 continuous
 *
 * @param x      Current state value (Q16.16)
 * @param x_min  Lower bound constraint (Q16.16)
 * @return       Barrier energy contribution (Q16.16), always >= 0
 */
static inline fixed_t fixed_barrier_potential(fixed_t x, fixed_t x_min) {
    /* dx = x - x_min + epsilon */
    fixed_t dx = barrier_fixed_sub(x, x_min);
    dx = barrier_fixed_add(dx, BARRIER_EPS);

    /* Violation check: if dx <= 0, we're at or past the bound */
    if (dx <= 0) {
        /* Return maximum penalty (clamped to prevent overflow) */
        return (fixed_t)0x7FFFFFFF;
    }

    /* U = kappa / dx */
    fixed_t U = barrier_fixed_div((fixed_t)BARRIER_STRENGTH, dx);

    return U;
}

/**
 * Compute barrier gradient (force contribution to state derivative).
 *
 * dU/dx = -kappa / (x - x_min + epsilon)^2
 *
 * This gradient should be ADDED to the state derivative to enforce
 * the constraint through an energetic penalty. The negative sign
 * creates a repulsive force away from the constraint boundary.
 *
 * @param x      Current state value (Q16.16)
 * @param x_min  Lower bound constraint (Q16.16)
 * @return       Barrier gradient (Q16.16), negative when near bound
 */
static inline fixed_t fixed_barrier_gradient(fixed_t x, fixed_t x_min) {
    /* dx = x - x_min + epsilon */
    fixed_t dx = barrier_fixed_sub(x, x_min);
    dx = barrier_fixed_add(dx, BARRIER_EPS);

    /* Violation check: if dx <= 0, return maximum repulsive gradient */
    if (dx <= 0) {
        /* Return large negative gradient to push state back */
        return (fixed_t)(-0x7FFFFFFF);
    }

    /* Compute inv = 1 / dx (Q16.16) */
    fixed_t inv = barrier_fixed_div(FRACUNIT, dx);

    /* Compute inv_sq = inv * inv (this is (1/dx)^2 in Q16.16) */
    fixed_t inv_sq = barrier_fixed_mul(inv, inv);

    /* grad = -kappa * inv_sq */
    fixed_t grad = barrier_fixed_mul((fixed_t)BARRIER_STRENGTH, inv_sq);

    /* Return negative gradient (repulsive force away from bound) */
    return -grad;
}

/**
 * Compute barrier gradient for upper bound constraint.
 *
 * For an upper bound x_max, the barrier potential is:
 *   U_barrier(x) = kappa / (x_max - x + epsilon)
 *
 * And the gradient is:
 *   dU/dx = kappa / (x_max - x + epsilon)^2
 *
 * Note: positive gradient pushes state down, away from upper bound.
 *
 * @param x      Current state value (Q16.16)
 * @param x_max  Upper bound constraint (Q16.16)
 * @return       Barrier gradient (Q16.16), positive when near bound
 */
static inline fixed_t fixed_barrier_gradient_upper(fixed_t x, fixed_t x_max) {
    /* dx = x_max - x + epsilon */
    fixed_t dx = barrier_fixed_sub(x_max, x);
    dx = barrier_fixed_add(dx, BARRIER_EPS);

    /* Violation check */
    if (dx <= 0) {
        /* Return large positive gradient to push state back down */
        return (fixed_t)0x7FFFFFFF;
    }

    /* Compute inv = 1 / dx */
    fixed_t inv = barrier_fixed_div(FRACUNIT, dx);

    /* Compute inv_sq = inv * inv */
    fixed_t inv_sq = barrier_fixed_mul(inv, inv);

    /* grad = kappa * inv_sq (positive to push down) */
    fixed_t grad = barrier_fixed_mul((fixed_t)BARRIER_STRENGTH, inv_sq);

    return grad;
}

/**
 * Combined barrier gradient for both lower and upper bounds.
 *
 * For state x bounded by [x_min, x_max], computes the total
 * barrier gradient contribution from both constraints.
 *
 * @param x      Current state value (Q16.16)
 * @param x_min  Lower bound constraint (Q16.16)
 * @param x_max  Upper bound constraint (Q16.16)
 * @return       Combined barrier gradient (Q16.16)
 */
static inline fixed_t fixed_barrier_gradient_bounded(fixed_t x, fixed_t x_min, fixed_t x_max) {
    fixed_t grad_lower = fixed_barrier_gradient(x, x_min);
    fixed_t grad_upper = fixed_barrier_gradient_upper(x, x_max);
    return barrier_fixed_add(grad_lower, grad_upper);
}

/* ========================================================================
 * CONVERSION UTILITIES
 * ======================================================================== */

/**
 * Convert float to Q16.16 fixed-point.
 * Rounds to nearest representable value.
 */
static inline fixed_t float_to_barrier_fixed(float f) {
    return (fixed_t)(f * (float)FRACUNIT + (f >= 0.0f ? 0.5f : -0.5f));
}

/**
 * Convert Q16.16 fixed-point to float.
 */
static inline float barrier_fixed_to_float(fixed_t fx) {
    return (float)fx / (float)FRACUNIT;
}

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BARRIERS_H */
