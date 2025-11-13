/*
 * se3_edge.h - Doom-Inspired SE(3) λ-Estimation for ESP32-S3
 *
 * Master header for fixed-point SE(3) spatial tracking and regenerative metrics.
 * Replicate 1993 Doom engine deterministic math for edge devices.
 *
 * Hardware Target: Unexpected Maker ProS3 ESP32-S3
 *   - Dual Xtensa LX7 @ 240MHz
 *   - 8MB PSRAM + 512KB SRAM
 *   - No FPU required (fixed-point only)
 *
 * Reference: id-Software/DOOM linuxdoom-1.10/m_fixed.h
 * Author: ClaudeCode (Doom→SE(3) λ-Estimation Service)
 * Version: 1.0
 */

#ifndef SE3_EDGE_H
#define SE3_EDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * FIXED-POINT MATHEMATICS (Doom-compatible)
 * ======================================================================== */

/**
 * Fixed-point format: 16.16 (signed 32-bit)
 *   - Integer part: 16 bits (range: -32768 to 32767)
 *   - Fractional part: 16 bits (precision: ~1.5e-5)
 *   - FRACUNIT = 65,536 represents 1.0
 */
#define FRACBITS 16
#define FRACUNIT (1 << FRACBITS)  /* 65,536 */

typedef int32_t fixed_t;

/* Conversion macros */
#define INT_TO_FIXED(i)     ((fixed_t)((i) << FRACBITS))
#define FLOAT_TO_FIXED(f)   ((fixed_t)((f) * FRACUNIT))
#define FIXED_TO_INT(f)     ((f) >> FRACBITS)
#define FIXED_TO_FLOAT(f)   ((float)(f) / FRACUNIT)

/* Fixed-point arithmetic (with 64-bit intermediates) */

/**
 * Fixed-point multiplication: a * b
 *
 * Uses 64-bit intermediate to prevent overflow.
 * Error budget: ±1 LSB (~1.5e-5)
 *
 * @param a First operand (16.16 fixed-point)
 * @param b Second operand (16.16 fixed-point)
 * @return Product (16.16 fixed-point)
 */
static inline fixed_t FixedMul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) >> FRACBITS);
}

/**
 * Fixed-point division: a / b
 *
 * Uses 64-bit intermediate to prevent overflow.
 * Saturates on divide-by-zero (no exceptions).
 *
 * @param a Numerator (16.16 fixed-point)
 * @param b Denominator (16.16 fixed-point)
 * @return Quotient (16.16 fixed-point), saturated if b == 0
 */
static inline fixed_t FixedDiv(fixed_t a, fixed_t b) {
    if (b == 0) {
        /* Saturate to max/min on divide-by-zero */
        return (a < 0) ? INT32_MIN : INT32_MAX;
    }
    return (fixed_t)(((int64_t)a << FRACBITS) / (int64_t)b);
}

/* ========================================================================
 * TRIGONOMETRIC LOOKUP TABLES (Doom-style)
 * ======================================================================== */

/**
 * Angle representation: 32-bit unsigned (0 to 0xFFFFFFFF = 0° to 360°)
 *   - Full circle: 0xFFFFFFFF + 1 = 0x00000000
 *   - 90 degrees:  0x40000000
 *   - 180 degrees: 0x80000000
 *   - 270 degrees: 0xC0000000
 */
#define ANGLE_BITS 13
#define NUM_FINE_ANGLES (1 << ANGLE_BITS)  /* 8192 entries */
#define ANGLE_MASK (NUM_FINE_ANGLES - 1)

/**
 * Sine lookup table (from generated trig_tables.h)
 *
 * Resolution: ~0.044° per entry
 * Memory: 32 KB
 */
extern const fixed_t finesine[NUM_FINE_ANGLES];

/**
 * Cosine via pointer offset (cos(x) = sin(x + 90°))
 *
 * Doom optimization: no separate cosine table needed.
 */
extern const fixed_t* const finecosine;

/**
 * Sine from LUT using 32-bit angle.
 *
 * @param angle 32-bit angle (0x00000000 = 0°, 0xFFFFFFFF = ~360°)
 * @return sin(angle) in 16.16 fixed-point [-1.0, 1.0]
 */
static inline fixed_t Sin_from_LUT(uint32_t angle) {
    return finesine[(angle >> (32 - ANGLE_BITS)) & ANGLE_MASK];
}

/**
 * Cosine from LUT using 32-bit angle.
 *
 * cos(x) = sin(x + 90°), so add 90° (quarter rotation) to angle.
 *
 * @param angle 32-bit angle (0x00000000 = 0°, 0xFFFFFFFF = ~360°)
 * @return cos(angle) in 16.16 fixed-point [-1.0, 1.0]
 */
static inline fixed_t Cos_from_LUT(uint32_t angle) {
    /* Add 90° (0x40000000 = 1/4 of 2^32) and lookup in sine table */
    uint32_t angle_plus_90 = angle + 0x40000000;
    return finesine[(angle_plus_90 >> (32 - ANGLE_BITS)) & ANGLE_MASK];
}

/* ========================================================================
 * SE(3) DATA STRUCTURES (ESP32-S3 optimized)
 * ======================================================================== */

/**
 * SE(3) pose: 3D position + orientation
 *
 * Coordinate frame (ENU):
 *   - East  = +X
 *   - North = +Y
 *   - Up    = +Z
 *
 * Rotation matrix: 3x3 row-major format
 *   R = [r00 r01 r02]
 *       [r10 r11 r12]
 *       [r20 r21 r22]
 *
 * GPS heading conversion:
 *   - GPS: 0° = North, 90° = East (compass convention)
 *   - SE(3): 0° = East, 90° = North (math convention)
 *   - Correction: yaw = (heading + 90) % 360
 */
#pragma pack(push, 1)  /* ESP32 alignment */
typedef struct {
    fixed_t rotation[9];     /* 3x3 matrix, row-major (36 bytes) */
    fixed_t translation[3];  /* ENU meters * FRACUNIT (12 bytes) */
    uint32_t timestamp;      /* Unix epoch seconds (4 bytes) */
    uint32_t mmsi;           /* Maritime Mobile Service Identity (4 bytes) */
} se3_pose_t;              /* Total: 56 bytes */
#pragma pack(pop)

/* ========================================================================
 * CELL HANDOFF PROTOCOL (vessel transitions)
 * ======================================================================== */

/* Note: T-BSP types (t_bsp_t, t_bsp_cell_t) are defined in t_bsp.h */

/**
 * Handoff packet: vessel moving from one cell to another.
 *
 * Flags:
 *   - Bit 0: dateline_cross (longitude wraps at ±180°)
 *   - Bit 1: polar_region (latitude near ±90°)
 *   - Bits 2-7: reserved
 */
#define HANDOFF_FLAG_DATELINE_CROSS  (1 << 0)
#define HANDOFF_FLAG_POLAR_REGION    (1 << 1)

#pragma pack(push, 1)
typedef struct {
    uint32_t mmsi;          /* Vessel identifier (4 bytes) */
    se3_pose_t last_pose;   /* Final pose in old cell (56 bytes) */
    uint16_t old_cell_id;   /* Source cell (2 bytes) */
    uint16_t new_cell_id;   /* Destination cell (2 bytes) */
    uint8_t flags;          /* Handoff flags (1 byte) */
    uint8_t _padding[3];    /* Alignment (3 bytes) */
    uint8_t signature[32];  /* Optional ed25519 signature (32 bytes) */
} handoff_packet_t;         /* Total: 100 bytes */
#pragma pack(pop)

/* ========================================================================
 * DLT RECORD STRUCTURE (IOTA Tangle)
 * ======================================================================== */

/**
 * λ-estimation record for blockchain publication.
 *
 * Published to IOTA Streams:
 *   - Cell ID maps to IOTA channel
 *   - Trajectory hash ensures data integrity
 *   - Signature enables trustless verification
 */
typedef struct {
    char dataset[32];           /* "MarineCadastre_AIS" (32 bytes) */
    uint32_t mmsi;              /* Vessel identifier (4 bytes) */
    uint16_t cell_id;           /* Spatial partition (2 bytes) */
    uint16_t _padding;          /* Alignment (2 bytes) */
    fixed_t lambda_optimal;     /* Estimated λ (4 bytes) */
    fixed_t return_error;       /* Regenerative error metric (4 bytes) */
    uint8_t trajectory_hash[32];/* SHA256 of pose sequence (32 bytes) */
    uint32_t timestamp;         /* Unix epoch seconds (4 bytes) */
    uint8_t signature[64];      /* ed25519 signature (64 bytes) */
} dlt_record_t;                 /* Total: 148 bytes */

/* ========================================================================
 * FUNCTION DECLARATIONS
 * ======================================================================== */

/* Fixed-point math (se3_math.c) */
void se3_init_tables(void);
fixed_t normalize_lon(fixed_t lon);
void rotation_identity(fixed_t R[9]);
void rotation_from_yaw(uint32_t yaw, fixed_t R[9]);
uint32_t heading_to_angle(fixed_t heading_deg);
void rotation_mul(const fixed_t A[9], const fixed_t B[9], fixed_t C[9]);
fixed_t rotation_trace(const fixed_t R[9]);
fixed_t vec3_norm_squared(const fixed_t v[3]);
void vec3_sub(const fixed_t a[3], const fixed_t b[3], fixed_t result[3]);
void mat3_mul_vec3(const fixed_t R[9], const fixed_t v[3], fixed_t result[3]);
void se3_pose_identity(se3_pose_t* pose);
void se3_pose_from_gps(fixed_t east, fixed_t north, fixed_t up,
                       fixed_t heading_deg, uint32_t timestamp,
                       uint32_t mmsi, se3_pose_t* pose);
fixed_t fixed_abs(fixed_t val);
fixed_t fixed_saturate(fixed_t val, fixed_t min_val, fixed_t max_val);
bool fixed_in_range(fixed_t val, fixed_t min_val, fixed_t max_val);

/* Trigonometric LUT validation (trig_tables.c) */
fixed_t get_sine_table_entry(uint16_t index);
fixed_t get_cosine_table_entry(uint16_t index);
fixed_t verify_pythagorean_identity(uint32_t angle);
fixed_t get_max_pythagorean_error(void);
fixed_t Sin_from_LUT_interp(uint32_t angle);
fixed_t Cos_from_LUT_interp(uint32_t angle);

/* Spatial partitioning (t_bsp.c) - t_bsp_cell_t already defined above */
/* See t_bsp.h for full T-BSP API */

/* Handoff protocol (handoff.c) */
void serialize_handoff(const handoff_packet_t* packet, uint8_t* buffer);
bool deserialize_handoff(const uint8_t* buffer, handoff_packet_t* packet);
bool handoff_should_trigger(const se3_pose_t* prev, const se3_pose_t* curr);
void create_handoff_packet(uint32_t mmsi, const se3_pose_t* last_pose,
                           uint16_t old_cell_id, uint16_t new_cell_id,
                           uint8_t flags, handoff_packet_t* pkt);
bool detect_dateline_cross(fixed_t lon1, fixed_t lon2);
uint8_t compute_handoff_flags(fixed_t lat1, fixed_t lon1, fixed_t lat2, fixed_t lon2);
size_t get_handoff_packet_size(void);
bool validate_handoff_packet(const handoff_packet_t* pkt, uint32_t current_time);

/* λ-estimation (lambda_estimator.c) */
fixed_t compute_return_error(const se3_pose_t* poses, int n, fixed_t lambda);
fixed_t adjust_lambda(fixed_t lambda, fixed_t error);
fixed_t fast_lambda_estimate(const se3_pose_t* poses, int n, fixed_t eps, int max_iter);

/* DLT integration (record_lambda.c) */
void compute_trajectory_hash(const se3_pose_t* poses, int n, uint8_t* hash);
bool publish_lambda_record(const dlt_record_t* record);

/* ========================================================================
 * CONSTANTS AND THRESHOLDS
 * ======================================================================== */

/* λ-estimation thresholds */
#define LAMBDA_EPSILON       FLOAT_TO_FIXED(0.001f)  /* 0.1% target error */
#define LAMBDA_VARIANCE_MAX  FLOAT_TO_FIXED(0.005f)  /* Statistical stability */
#define LAMBDA_MAX_ITER      12                       /* Iteration budget */

/* Geodetic constants (fixed-point degrees) */
#define FIXED_180_DEG        FLOAT_TO_FIXED(180.0f)
#define FIXED_360_DEG        FLOAT_TO_FIXED(360.0f)
#define FIXED_90_DEG         FLOAT_TO_FIXED(90.0f)

/* Grid levels (fixed-point km) */
#define GRID_LEVEL_0         FLOAT_TO_FIXED(100.0f)  /* Open ocean */
#define GRID_LEVEL_1         FLOAT_TO_FIXED(10.0f)   /* Coastal */
#define GRID_LEVEL_2         FLOAT_TO_FIXED(1.0f)    /* Ports */

#ifdef __cplusplus
}
#endif

#endif /* SE3_EDGE_H */
