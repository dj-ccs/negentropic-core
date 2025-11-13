/*
 * se3_math.c - Fixed-Point Mathematics for SE(3) Edge Computation
 *
 * Doom-inspired deterministic arithmetic for ESP32-S3 (no FPU required).
 * All operations use 16.16 fixed-point format with 64-bit intermediates.
 *
 * Reference: id-Software/DOOM linuxdoom-1.10/m_fixed.c
 * Author: ClaudeCode (Doom→SE(3) λ-Estimation Service)
 * Version: 1.0
 */

#include "se3_edge.h"
#include <string.h>

/* ========================================================================
 * INITIALIZATION
 * ======================================================================== */

/**
 * Initialize SE(3) math subsystem.
 *
 * Currently a placeholder for future hardware-specific optimizations:
 *   - ESP32-S3 SIMD instructions (if available)
 *   - Cache prefetching hints
 *   - FreeRTOS task affinity
 */
void se3_init_tables(void) {
    /* Tables are statically generated - no runtime initialization needed */
    /* Future: could verify table integrity via checksum */
}

/* ========================================================================
 * GEODETIC UTILITIES
 * ======================================================================== */

/**
 * Normalize longitude to [-180°, 180°] range.
 *
 * Handles International Date Line crossing (±180° wraparound).
 * Critical for T-BSP cell assignment near dateline.
 *
 * @param lon Longitude in fixed-point degrees
 * @return Normalized longitude in [-180°, 180°]
 */
fixed_t normalize_lon(fixed_t lon) {
    /* Wrap longitude to [-180, 180] */
    while (lon > FIXED_180_DEG) {
        lon -= FIXED_360_DEG;
    }
    while (lon < -FIXED_180_DEG) {
        lon += FIXED_360_DEG;
    }
    return lon;
}

/* ========================================================================
 * ROTATION MATRIX OPERATIONS
 * ======================================================================== */

/**
 * Create identity rotation matrix (3x3).
 *
 * @param R Output 9-element array (row-major)
 */
void rotation_identity(fixed_t R[9]) {
    memset(R, 0, 9 * sizeof(fixed_t));
    R[0] = FRACUNIT;  /* R[0][0] = 1.0 */
    R[4] = FRACUNIT;  /* R[1][1] = 1.0 */
    R[8] = FRACUNIT;  /* R[2][2] = 1.0 */
}

/**
 * Create 2D rotation matrix from yaw angle (rotation about Z-axis).
 *
 * For surface vessels, this is the primary rotation component.
 * Embeds 2D rotation into 3D matrix:
 *   [cos -sin  0]
 *   [sin  cos  0]
 *   [0    0    1]
 *
 * @param yaw Yaw angle (32-bit unsigned: 0x00000000 = 0°, 0xFFFFFFFF = ~360°)
 * @param R Output 9-element rotation matrix (row-major)
 */
void rotation_from_yaw(uint32_t yaw, fixed_t R[9]) {
    fixed_t cos_yaw = Cos_from_LUT(yaw);
    fixed_t sin_yaw = Sin_from_LUT(yaw);

    R[0] =  cos_yaw;   R[1] = -sin_yaw;   R[2] = 0;
    R[3] =  sin_yaw;   R[4] =  cos_yaw;   R[5] = 0;
    R[6] =  0;         R[7] =  0;         R[8] = FRACUNIT;
}

/**
 * Convert GPS heading to 32-bit angle (with coordinate frame correction).
 *
 * GPS heading convention:
 *   - 0° = North, 90° = East, 180° = South, 270° = West
 *
 * SE(3) convention (ENU frame):
 *   - 0° = East, 90° = North, 180° = West, 270° = South
 *
 * Correction: SE(3) angle = (GPS heading + 90°) mod 360°
 *
 * @param heading_deg GPS heading in fixed-point degrees [0, 360)
 * @return 32-bit angle for LUT (0x00000000 to 0xFFFFFFFF)
 */
uint32_t heading_to_angle(fixed_t heading_deg) {
    /* Apply +90° coordinate frame correction */
    fixed_t corrected_deg = heading_deg + FIXED_90_DEG;

    /* Wrap to [0, 360) */
    while (corrected_deg >= FIXED_360_DEG) {
        corrected_deg -= FIXED_360_DEG;
    }
    while (corrected_deg < 0) {
        corrected_deg += FIXED_360_DEG;
    }

    /* Convert to 32-bit angle: angle = (degrees / 360) * 2^32 */
    /* Using 64-bit intermediate: angle = (degrees * 2^32) / 360 */
    uint64_t numerator = (uint64_t)corrected_deg << 32;
    uint32_t angle = (uint32_t)(numerator / FIXED_360_DEG);

    return angle;
}

/**
 * Multiply two 3x3 rotation matrices: C = A * B.
 *
 * Row-major format multiplication.
 * Used for rotation composition in long trajectories.
 *
 * Warning: Accumulated error in long chains (>100 rotations).
 * Monitor error and renormalize if needed.
 *
 * @param A First rotation matrix (9 elements)
 * @param B Second rotation matrix (9 elements)
 * @param C Output rotation matrix (9 elements, can be same as A or B)
 */
void rotation_mul(const fixed_t A[9], const fixed_t B[9], fixed_t C[9]) {
    /* Temporary buffer to allow C = A or C = B */
    fixed_t temp[9];

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int64_t sum = 0;
            for (int k = 0; k < 3; k++) {
                sum += (int64_t)A[i*3 + k] * (int64_t)B[k*3 + j];
            }
            temp[i*3 + j] = (fixed_t)(sum >> FRACBITS);
        }
    }

    memcpy(C, temp, 9 * sizeof(fixed_t));
}

/**
 * Compute trace of 3x3 rotation matrix: tr(R) = R[0] + R[4] + R[8].
 *
 * Used for rotation error estimation via exponential map:
 *   θ = acos((trace - 1) / 2)
 *
 * For identity matrix: trace = 3.0 (in fixed-point: 3 * FRACUNIT)
 *
 * @param R Rotation matrix (9 elements)
 * @return Trace (fixed-point)
 */
fixed_t rotation_trace(const fixed_t R[9]) {
    return R[0] + R[4] + R[8];
}

/* ========================================================================
 * VECTOR OPERATIONS (3D)
 * ======================================================================== */

/**
 * Euclidean norm squared: ||v||^2 = v[0]^2 + v[1]^2 + v[2]^2.
 *
 * Avoids square root for efficiency.
 * Sufficient for distance comparisons and error metrics.
 *
 * @param v 3-element vector (fixed-point)
 * @return Squared norm (fixed-point)
 */
fixed_t vec3_norm_squared(const fixed_t v[3]) {
    fixed_t v0_sq = FixedMul(v[0], v[0]);
    fixed_t v1_sq = FixedMul(v[1], v[1]);
    fixed_t v2_sq = FixedMul(v[2], v[2]);
    return v0_sq + v1_sq + v2_sq;
}

/**
 * Vector subtraction: result = a - b.
 *
 * @param a First vector (3 elements)
 * @param b Second vector (3 elements)
 * @param result Output vector (3 elements, can be same as a or b)
 */
void vec3_sub(const fixed_t a[3], const fixed_t b[3], fixed_t result[3]) {
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

/**
 * Matrix-vector multiplication: result = R * v (3x3 matrix, 3D vector).
 *
 * @param R Rotation matrix (9 elements, row-major)
 * @param v Input vector (3 elements)
 * @param result Output vector (3 elements, can be same as v)
 */
void mat3_mul_vec3(const fixed_t R[9], const fixed_t v[3], fixed_t result[3]) {
    fixed_t temp[3];

    for (int i = 0; i < 3; i++) {
        int64_t sum = 0;
        for (int j = 0; j < 3; j++) {
            sum += (int64_t)R[i*3 + j] * (int64_t)v[j];
        }
        temp[i] = (fixed_t)(sum >> FRACBITS);
    }

    result[0] = temp[0];
    result[1] = temp[1];
    result[2] = temp[2];
}

/* ========================================================================
 * SE(3) POSE UTILITIES
 * ======================================================================== */

/**
 * Initialize SE(3) pose to identity (origin with no rotation).
 *
 * @param pose Output pose structure
 */
void se3_pose_identity(se3_pose_t* pose) {
    rotation_identity(pose->rotation);
    pose->translation[0] = 0;
    pose->translation[1] = 0;
    pose->translation[2] = 0;
    pose->timestamp = 0;
    pose->mmsi = 0;
}

/**
 * Create SE(3) pose from GPS data (lat, lon, heading).
 *
 * Coordinate transformations:
 *   1. WGS84 (lat/lon) → ENU (East-North-Up) meters
 *   2. GPS heading → SE(3) yaw rotation (with +90° correction)
 *
 * Note: ENU conversion requires reference point (not implemented here).
 * This function assumes ENU coordinates are pre-computed.
 *
 * @param east East coordinate in ENU frame (meters, fixed-point)
 * @param north North coordinate in ENU frame (meters, fixed-point)
 * @param up Up coordinate in ENU frame (meters, fixed-point, usually 0)
 * @param heading_deg GPS heading (degrees, fixed-point)
 * @param timestamp Unix epoch seconds
 * @param mmsi Vessel identifier
 * @param pose Output SE(3) pose
 */
void se3_pose_from_gps(fixed_t east, fixed_t north, fixed_t up,
                       fixed_t heading_deg, uint32_t timestamp,
                       uint32_t mmsi, se3_pose_t* pose) {
    /* Convert heading to rotation matrix */
    uint32_t angle = heading_to_angle(heading_deg);
    rotation_from_yaw(angle, pose->rotation);

    /* Set translation */
    pose->translation[0] = east;
    pose->translation[1] = north;
    pose->translation[2] = up;

    /* Set metadata */
    pose->timestamp = timestamp;
    pose->mmsi = mmsi;
}

/* ========================================================================
 * DIAGNOSTIC UTILITIES
 * ======================================================================== */

/**
 * Check if fixed-point value is within valid range.
 *
 * Useful for detecting overflow/saturation in long computations.
 *
 * @param val Fixed-point value
 * @param min_val Minimum allowed value
 * @param max_val Maximum allowed value
 * @return true if val ∈ [min_val, max_val], false otherwise
 */
bool fixed_in_range(fixed_t val, fixed_t min_val, fixed_t max_val) {
    return (val >= min_val) && (val <= max_val);
}

/**
 * Compute absolute value of fixed-point number.
 *
 * @param val Fixed-point value
 * @return |val|
 */
fixed_t fixed_abs(fixed_t val) {
    return (val < 0) ? -val : val;
}

/**
 * Saturate fixed-point value to range.
 *
 * @param val Input value
 * @param min_val Minimum (saturate lower bound)
 * @param max_val Maximum (saturate upper bound)
 * @return Saturated value
 */
fixed_t fixed_saturate(fixed_t val, fixed_t min_val, fixed_t max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}
