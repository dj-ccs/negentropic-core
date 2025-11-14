/*
 * se3_types.h - SE(3) Type Definitions
 *
 * Defines explicit SE(3) types for double-precision and fixed-point
 * arithmetic to remove ambiguity.
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#ifndef NEG_SE3_TYPES_H
#define NEG_SE3_TYPES_H

#include <stdint.h>
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * FIXED-POINT SE(3) (16.16 format)
 * ======================================================================== */

/**
 * Fixed-point SE(3) pose (16.16 format).
 *
 * Used for embedded systems and deterministic computation.
 * Coordinate frame: ENU (East-North-Up)
 */
typedef struct NEG_ALIGN16 {
    int32_t rotation[9];     /* 3x3 rotation matrix (16.16 fixed-point) */
    int32_t translation[3];  /* Translation vector (16.16 fixed-point) */
    uint32_t timestamp;      /* Unix epoch seconds */
    uint32_t entity_id;      /* Entity identifier */
} SE3fxp;

/* ========================================================================
 * DOUBLE-PRECISION SE(3)
 * ======================================================================== */

/**
 * Double-precision SE(3) pose.
 *
 * Used for high-precision computation and reference implementations.
 * Coordinate frame: ENU (East-North-Up)
 */
typedef struct NEG_ALIGN32 {
    double rotation[9];      /* 3x3 rotation matrix (double precision) */
    double translation[3];   /* Translation vector (double precision) */
    uint64_t timestamp;      /* Microseconds since Unix epoch */
    uint32_t entity_id;      /* Entity identifier */
    uint32_t _padding;       /* Alignment padding */
} SE3d;

/* ========================================================================
 * SINGLE-PRECISION SE(3) (for Unity/GPU)
 * ======================================================================== */

/**
 * Single-precision SE(3) pose.
 *
 * Used for Unity integration and GPU computation.
 * Coordinate frame: ENU (East-North-Up)
 */
typedef struct NEG_ALIGN16 {
    float rotation[9];       /* 3x3 rotation matrix (single precision) */
    float translation[3];    /* Translation vector (single precision) */
    uint64_t timestamp;      /* Microseconds since Unix epoch */
    uint32_t entity_id;      /* Entity identifier */
    uint32_t _padding;       /* Alignment padding */
} SE3f;

/* ========================================================================
 * CONVERSION FUNCTIONS
 * ======================================================================== */

/**
 * Convert fixed-point SE(3) to double-precision.
 */
void se3_fxp_to_double(const SE3fxp* src, SE3d* dst);

/**
 * Convert double-precision SE(3) to fixed-point.
 */
void se3_double_to_fxp(const SE3d* src, SE3fxp* dst);

/**
 * Convert single-precision SE(3) to double-precision.
 */
void se3_float_to_double(const SE3f* src, SE3d* dst);

/**
 * Convert double-precision SE(3) to single-precision.
 */
void se3_double_to_float(const SE3d* src, SE3f* dst);

#ifdef __cplusplus
}
#endif

#endif /* NEG_SE3_TYPES_H */
