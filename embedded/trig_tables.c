/*
 * trig_tables.c - Trigonometric Lookup Tables for SE(3) Edge
 *
 * Embeds Doom-style sine/cosine tables for fixed-point trigonometry.
 * Tables are generated at build time by tools/generate_trig_lut.py.
 *
 * Memory footprint: 32 KB (8192 entries × 4 bytes)
 * Resolution: ~0.044° per entry
 *
 * Reference: id-Software/DOOM linuxdoom-1.10/tables.c
 * Author: ClaudeCode (Doom→SE(3) λ-Estimation Service)
 * Version: 1.0
 */

#include "se3_edge.h"

/* ========================================================================
 * EXPORTED TABLES (defined in generated trig_tables.h)
 * ======================================================================== */

/**
 * Include the generated lookup table data.
 * This defines:
 *   - const fixed_t finesine[NUM_FINE_ANGLES]
 *   - const fixed_t* const finecosine
 *
 * These have external linkage and can be used from other compilation units.
 */
#include "trig_tables.h"

/* ========================================================================
 * VALIDATION FUNCTIONS (for unit tests)
 * ======================================================================== */

/**
 * Get raw sine table value by index (for testing).
 *
 * @param index Table index [0, NUM_FINE_ANGLES)
 * @return Sine value (fixed-point)
 */
fixed_t get_sine_table_entry(uint16_t index) {
    if (index >= NUM_FINE_ANGLES) {
        return 0;  /* Out of bounds */
    }
    return finesine[index];
}

/**
 * Get raw cosine table value by index (for testing).
 *
 * @param index Table index [0, NUM_FINE_ANGLES)
 * @return Cosine value (fixed-point)
 */
fixed_t get_cosine_table_entry(uint16_t index) {
    if (index >= NUM_FINE_ANGLES) {
        return 0;  /* Out of bounds */
    }
    return finecosine[index];
}

/**
 * Verify sin² + cos² = 1 for a given angle (Pythagorean identity).
 *
 * Used in unit tests to validate LUT accuracy.
 * Returns error magnitude (should be < 0.001 in fixed-point).
 *
 * @param angle 32-bit angle
 * @return |sin²(angle) + cos²(angle) - 1| (fixed-point)
 */
fixed_t verify_pythagorean_identity(uint32_t angle) {
    fixed_t sin_val = Sin_from_LUT(angle);
    fixed_t cos_val = Cos_from_LUT(angle);

    fixed_t sin_sq = FixedMul(sin_val, sin_val);
    fixed_t cos_sq = FixedMul(cos_val, cos_val);

    fixed_t sum = sin_sq + cos_sq;
    fixed_t error = sum - FRACUNIT;  /* Should be ~0 */

    return (error < 0) ? -error : error;  /* Absolute value */
}

/**
 * Get maximum Pythagorean identity error across all table entries.
 *
 * Useful for validating overall LUT quality.
 * Target: max_error < 0.001 (LAMBDA_EPSILON)
 *
 * @return Maximum error (fixed-point)
 */
fixed_t get_max_pythagorean_error(void) {
    fixed_t max_error = 0;

    for (uint16_t i = 0; i < NUM_FINE_ANGLES; i++) {
        /* Convert index to 32-bit angle */
        uint32_t angle = ((uint32_t)i << (32 - ANGLE_BITS));

        fixed_t error = verify_pythagorean_identity(angle);
        if (error > max_error) {
            max_error = error;
        }
    }

    return max_error;
}

/* ========================================================================
 * INTERPOLATION (optional enhancement for higher accuracy)
 * ======================================================================== */

/**
 * Linear interpolation between two LUT entries.
 *
 * For applications requiring >0.044° accuracy, this provides
 * intermediate values between table entries.
 *
 * Performance: ~3x slower than direct LUT (still faster than math.h sin())
 *
 * @param angle 32-bit angle
 * @return Interpolated sine value (fixed-point)
 */
fixed_t Sin_from_LUT_interp(uint32_t angle) {
    /* Extract high 13 bits for table index */
    uint32_t shifted = angle >> (32 - ANGLE_BITS);
    uint16_t index_low = (uint16_t)(shifted & ANGLE_MASK);
    uint16_t index_high = (index_low + 1) & ANGLE_MASK;

    /* Extract fractional part for interpolation (next 16 bits) */
    uint32_t frac_bits = (angle >> (32 - ANGLE_BITS - 16)) & 0xFFFF;
    fixed_t frac = (fixed_t)frac_bits;  /* Already in Q16 format */

    /* Linear interpolation: val = low + frac * (high - low) */
    fixed_t val_low = finesine[index_low];
    fixed_t val_high = finesine[index_high];
    fixed_t delta = val_high - val_low;

    return val_low + FixedMul(frac, delta);
}

/**
 * Interpolated cosine (optional enhancement).
 *
 * @param angle 32-bit angle
 * @return Interpolated cosine value (fixed-point)
 */
fixed_t Cos_from_LUT_interp(uint32_t angle) {
    /* Shift by 90° and use sine interpolation */
    uint32_t shifted_angle = angle + 0x40000000;  /* +90° = 1/4 of 2^32 */
    return Sin_from_LUT_interp(shifted_angle);
}
