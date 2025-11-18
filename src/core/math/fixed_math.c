/**
 * fixed_math.c - Doom Ethos Fixed-Point Math Implementation
 *
 * LUT-accelerated arithmetic with perfect cross-platform determinism.
 *
 * Author: ClaudeCode (v2.2 Doom Ethos Sprint)
 * Version: 1.0
 * Date: 2025-11-18
 */

#include "fixed_math.h"
#include <math.h>  // Only for LUT generation, not in hot paths
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * STATIC LUT STORAGE
 * ======================================================================== */

/**
 * Reciprocal LUT: 1/x for x ∈ [1.0, 256.0]
 * Generated at initialization with perfect FP64 precision.
 */
static fixed_t reciprocal_lut_data[RECIPROCAL_LUT_SIZE];

/**
 * Square root LUT: sqrt(x) for x ∈ [0.0, 1024.0]
 * Generated at initialization with perfect FP64 precision.
 */
static fixed_t sqrt_lut_data[SQRT_LUT_SIZE];

/* Export const pointers for external access */
const fixed_t* reciprocal_lut = reciprocal_lut_data;
const fixed_t* sqrt_lut = sqrt_lut_data;

/* Initialization flag */
static bool luts_initialized = false;

/* ========================================================================
 * LUT GENERATION (called once at startup)
 * ======================================================================== */

/**
 * Generate reciprocal LUT: 1/x for x ∈ [1.0, 256.0]
 *
 * Method: Direct computation using FP64, then round to Q16.16
 * Entries: reciprocal_lut[i] = 1.0 / (1.0 + i)
 */
static void generate_reciprocal_lut(void) {
    for (int i = 0; i < RECIPROCAL_LUT_SIZE; i++) {
        double x = 1.0 + (double)i;  // Range: [1.0, 256.0]
        double recip = 1.0 / x;
        reciprocal_lut_data[i] = FLOAT_TO_FIXED((float)recip);
    }
}

/**
 * Generate sqrt LUT: sqrt(x) for x ∈ [0.0, 1024.0]
 *
 * Method: Direct computation using FP64 sqrt, then round to Q16.16
 * Entries: sqrt_lut[i] = sqrt(2.0 * i)  (spaced for better coverage)
 */
static void generate_sqrt_lut(void) {
    for (int i = 0; i < SQRT_LUT_SIZE; i++) {
        double x = 2.0 * (double)i;  // Range: [0.0, 1024.0]
        double sqrt_val = sqrt(x);
        sqrt_lut_data[i] = FLOAT_TO_FIXED((float)sqrt_val);
    }
}

/* ========================================================================
 * PUBLIC LUT INITIALIZATION
 * ======================================================================== */

int fixed_math_init(void) {
    if (luts_initialized) {
        return 0;  // Already initialized
    }

    generate_reciprocal_lut();
    generate_sqrt_lut();

    luts_initialized = true;
    return 0;
}

float fixed_math_verify_lut(const char* lut_name) {
    if (!luts_initialized) {
        return -1.0f;  // Error: LUTs not initialized
    }

    float max_error = 0.0f;

    if (strcmp(lut_name, "reciprocal") == 0) {
        for (int i = 0; i < RECIPROCAL_LUT_SIZE; i++) {
            float x = 1.0f + (float)i;
            float expected = 1.0f / x;
            float actual = FIXED_TO_FLOAT(reciprocal_lut_data[i]);
            float error = fabsf((actual - expected) / expected);
            if (error > max_error) max_error = error;
        }
    } else if (strcmp(lut_name, "sqrt") == 0) {
        for (int i = 1; i < SQRT_LUT_SIZE; i++) {  // Skip i=0 to avoid divide by zero
            float x = 2.0f * (float)i;
            float expected = sqrtf(x);
            float actual = FIXED_TO_FLOAT(sqrt_lut_data[i]);
            float error = fabsf((actual - expected) / expected);
            if (error > max_error) max_error = error;
        }
    } else {
        return -1.0f;  // Unknown LUT name
    }

    return max_error;
}

/* ========================================================================
 * LUT-ACCELERATED FUNCTIONS
 * ======================================================================== */

fixed_t fixed_reciprocal(fixed_t x) {
    if (x == 0) return 0;  // Division by zero guard
    if (x < 0) x = -x;     // Handle negative (return positive reciprocal)

    // Check if x is in LUT range [1.0, 256.0]
    if (x < RECIPROCAL_MIN_VAL) {
        // x < 1.0: reciprocal is > 1.0, use direct division
        return FIXED_DIV(FRACUNIT, x);
    }
    if (x >= RECIPROCAL_MAX_VAL) {
        // x >= 256.0: reciprocal is < 1/256, use direct division
        return FIXED_DIV(FRACUNIT, x);
    }

    // Map x to LUT index with fractional part
    // x ranges from 1.0 to 256.0, map to [0, 255]
    int32_t x_shifted = x - FRACUNIT;  // Subtract 1.0
    int32_t index_fixed = x_shifted >> FIXED_SHIFT;  // Integer part
    int32_t frac = x_shifted & 0xFFFF;  // Fractional part

    // Clamp index to valid range
    if (index_fixed < 0) index_fixed = 0;
    if (index_fixed >= RECIPROCAL_LUT_SIZE - 1) index_fixed = RECIPROCAL_LUT_SIZE - 2;

    // Linear interpolation
    fixed_t v0 = reciprocal_lut_data[index_fixed];
    fixed_t v1 = reciprocal_lut_data[index_fixed + 1];
    fixed_t delta = v1 - v0;

    // result = v0 + frac * (v1 - v0)
    fixed_t interp = FIXED_MUL(INT_TO_FIXED(frac) >> FIXED_SHIFT, delta);
    return v0 + interp;
}

fixed_t fixed_sqrt(fixed_t x) {
    if (x <= 0) return 0;  // sqrt of negative is undefined

    // Check if x is in LUT range [0.0, 1024.0]
    if (x >= SQRT_MAX_VAL) {
        // x >= 1024.0: use iterative Newton-Raphson (2 iterations)
        // Initial guess: x / 2
        fixed_t guess = x >> 1;
        // Iteration 1: guess = (guess + x/guess) / 2
        fixed_t div1 = FIXED_DIV(x, guess);
        guess = (guess + div1) >> 1;
        // Iteration 2
        fixed_t div2 = FIXED_DIV(x, guess);
        guess = (guess + div2) >> 1;
        return guess;
    }

    // Map x to LUT index with fractional part
    // x ranges from 0.0 to 1024.0, LUT has 512 entries spaced by 2.0
    // index = x / 2.0
    int32_t index_fixed = x >> 1;  // Divide by 2.0 (shift right by FIXED_SHIFT+1, but we want Q16.16 index)
    index_fixed >>= FIXED_SHIFT;   // Get integer index
    int32_t frac = (x >> 1) & 0xFFFF;  // Fractional part

    // Clamp index to valid range
    if (index_fixed >= SQRT_LUT_SIZE - 1) index_fixed = SQRT_LUT_SIZE - 2;

    // Linear interpolation
    fixed_t v0 = sqrt_lut_data[index_fixed];
    fixed_t v1 = sqrt_lut_data[index_fixed + 1];
    fixed_t delta = v1 - v0;

    // result = v0 + frac * (v1 - v0)
    fixed_t interp = FIXED_MUL(INT_TO_FIXED(frac) >> FIXED_SHIFT, delta);
    return v0 + interp;
}

fixed_t fixed_inv_sqrt(fixed_t x) {
    if (x <= 0) return 0;

    // Quake III fast inverse square root adapted to Q16.16
    // Use Newton-Raphson: y = y * (1.5 - 0.5 * x * y * y)

    // Initial guess using bit hack (approximate)
    // Convert to float for magic constant trick
    float xf = FIXED_TO_FLOAT(x);
    int32_t i = *(int32_t*)&xf;
    i = 0x5f3759df - (i >> 1);  // Magic constant
    float yf = *(float*)&i;

    // One Newton-Raphson iteration
    yf = yf * (1.5f - 0.5f * xf * yf * yf);

    return FLOAT_TO_FIXED(yf);
}

fixed_t fixed_div_safe(fixed_t a, fixed_t b) {
    if (b == 0) return (a >= 0) ? INT32_MAX : INT32_MIN;  // Saturate on divide by zero

    int64_t result = (((int64_t)a) << FIXED_SHIFT) / b;

    // Saturate on overflow
    if (result > INT32_MAX) return INT32_MAX;
    if (result < INT32_MIN) return INT32_MIN;

    return (fixed_t)result;
}
