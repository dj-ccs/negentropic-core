/**
 * fixed_math.h - Doom Ethos Fixed-Point Math Library
 *
 * LUT-accelerated fixed-point arithmetic for cross-platform determinism.
 * All operations use Q16.16 format with pre-computed lookup tables.
 *
 * Performance targets (Doom Ethos):
 * - Reciprocal: <10 cycles (vs ~50 for division)
 * - Sqrt: <12 cycles (vs ~80 for float sqrt + conversion)
 * - Perfect determinism across x86-64, ARM, WASM, ESP32-S3
 *
 * Author: ClaudeCode (v2.2 Doom Ethos Sprint)
 * Version: 1.0
 * Date: 2025-11-18
 */

#ifndef NEG_FIXED_MATH_H
#define NEG_FIXED_MATH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * FIXED-POINT FORMAT SPECIFICATION
 * ======================================================================== */

/**
 * Q16.16 fixed-point format (from Deterministic_Numerics_Standard_v0.3.3.md)
 * Range: -32768.0 to +32767.99998474121
 * Precision: 1.52587890625e-05 (1/65536)
 */
typedef int32_t fixed_t;

#define FRACUNIT 65536              // 1.0 in Q16.16
#define FIXED_SHIFT 16
#define FIXED_ONE FRACUNIT

/* Basic conversions */
#define FLOAT_TO_FIXED(x) ((fixed_t)((x) * FRACUNIT + ((x) >= 0 ? 0.5f : -0.5f)))
#define FIXED_TO_FLOAT(x) ((float)(x) / FRACUNIT)
#define INT_TO_FIXED(x)   ((x) << FIXED_SHIFT)
#define FIXED_TO_INT(x)   ((x) >> FIXED_SHIFT)

/* Basic arithmetic */
#define FIXED_MUL(a, b) ((fixed_t)(((int64_t)(a) * (b)) >> FIXED_SHIFT))
#define FIXED_DIV(a, b) ((fixed_t)((((int64_t)(a)) << FIXED_SHIFT) / (b)))

/* ========================================================================
 * LUT SPECIFICATIONS
 * ======================================================================== */

/**
 * Reciprocal LUT: 1/x for x ∈ [1.0, 256.0]
 * Size: 256 entries × 4 bytes = 1 KB
 * Method: Linear interpolation for fractional indices
 * Accuracy: < 1e-4 relative error
 */
#define RECIPROCAL_LUT_SIZE 256
#define RECIPROCAL_MIN_VAL FRACUNIT        // 1.0
#define RECIPROCAL_MAX_VAL (256 * FRACUNIT)  // 256.0

/**
 * Square root LUT: sqrt(x) for x ∈ [0.0, 1024.0]
 * Size: 512 entries × 4 bytes = 2 KB
 * Method: Linear interpolation
 * Accuracy: < 1e-4 relative error
 */
#define SQRT_LUT_SIZE 512
#define SQRT_MAX_VAL (1024 * FRACUNIT)  // 1024.0

/* ========================================================================
 * LUT INITIALIZATION
 * ======================================================================== */

/**
 * Initialize all fixed-point LUTs.
 * Must be called once at startup before any fixed_* functions are used.
 *
 * Thread safety: Not thread-safe. Call from main thread before worker spawn.
 * Memory: Allocates 3 KB static data (reciprocal + sqrt LUTs)
 *
 * @return 0 on success, -1 on failure
 */
int fixed_math_init(void);

/**
 * Verify LUT accuracy (for unit tests).
 * Computes maximum relative error across all LUT entries.
 *
 * @param lut_name "reciprocal" or "sqrt"
 * @return Maximum relative error (0.0 to 1.0)
 */
float fixed_math_verify_lut(const char* lut_name);

/* ========================================================================
 * LUT-ACCELERATED FUNCTIONS
 * ======================================================================== */

/**
 * LUT-based reciprocal: 1/x
 *
 * Uses 256-entry lookup table with linear interpolation.
 * For x outside [1.0, 256.0], falls back to FIXED_DIV.
 *
 * Performance: ~10 cycles (vs ~50 for division)
 * Accuracy: < 1e-4 relative error within LUT range
 *
 * @param x Input value (Q16.16), must be > 0
 * @return 1/x (Q16.16), or 0 if x == 0
 */
fixed_t fixed_reciprocal(fixed_t x);

/**
 * LUT-based square root: sqrt(x)
 *
 * Uses 512-entry lookup table with linear interpolation.
 * For x outside [0.0, 1024.0], uses iterative refinement.
 *
 * Performance: ~12 cycles (vs ~80 for float sqrt)
 * Accuracy: < 1e-4 relative error within LUT range
 *
 * @param x Input value (Q16.16), must be >= 0
 * @return sqrt(x) (Q16.16), or 0 if x < 0
 */
fixed_t fixed_sqrt(fixed_t x);

/**
 * Fast inverse square root: 1/sqrt(x)
 *
 * Uses Quake III algorithm with one Newton-Raphson iteration.
 * Faster than fixed_reciprocal(fixed_sqrt(x)).
 *
 * Performance: ~15 cycles
 * Accuracy: < 5e-4 relative error
 *
 * @param x Input value (Q16.16), must be > 0
 * @return 1/sqrt(x) (Q16.16), or 0 if x <= 0
 */
fixed_t fixed_inv_sqrt(fixed_t x);

/**
 * Clamped division: a / b with saturation on overflow
 *
 * Safer than FIXED_DIV macro for untrusted inputs.
 * Saturates to INT32_MAX/MIN on overflow instead of wrapping.
 *
 * @param a Numerator (Q16.16)
 * @param b Denominator (Q16.16), must be != 0
 * @return a/b (Q16.16), saturated to [-32768, 32767]
 */
fixed_t fixed_div_safe(fixed_t a, fixed_t b);

/**
 * Check if multiplication will overflow
 *
 * @param a First operand (Q16.16)
 * @param b Second operand (Q16.16)
 * @return true if a*b would overflow int32_t
 */
static inline bool fixed_will_overflow_mul(fixed_t a, fixed_t b) {
    int64_t result = ((int64_t)a * (int64_t)b) >> 16;
    return result > INT32_MAX || result < INT32_MIN;
}

/* ========================================================================
 * EXTERNAL LUT TABLES (defined in fixed_math.c)
 * ======================================================================== */

extern const fixed_t reciprocal_lut[RECIPROCAL_LUT_SIZE];
extern const fixed_t sqrt_lut[SQRT_LUT_SIZE];

#ifdef __cplusplus
}
#endif

#endif /* NEG_FIXED_MATH_H */
