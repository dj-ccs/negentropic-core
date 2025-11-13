/*
 * state.h - Canonical Simulation State Representation
 *
 * Core state structure for negentropic-core deterministic physics kernel.
 * Designed for:
 *   - Memory efficiency (single contiguous block)
 *   - Deterministic hashing (binary reproducibility)
 *   - Multi-platform compatibility (Unity, WASM, ESP32)
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#ifndef NE_STATE_H
#define NE_STATE_H

#include <stdint.h>
#include <stddef.h>
#include "../../embedded/se3_edge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONFIGURATION STRUCTURES
 * ======================================================================== */

/**
 * Initial simulation configuration (parsed from JSON)
 */
typedef struct {
    uint32_t num_entities;          /* Number of SE(3) entities to track */
    uint32_t num_scalar_fields;     /* Number of scalar field values (temperature, moisture, etc.) */
    uint32_t grid_width;            /* Grid width (for spatial fields) */
    uint32_t grid_height;           /* Grid height (for spatial fields) */
    uint32_t grid_depth;            /* Grid depth (for 3D fields) */
    float dt;                       /* Default timestep (seconds) */
    uint8_t precision_mode;         /* 0=FP16, 1=FP32, 2=FP64, 3=Fixed-point */
    uint8_t integrator_type;        /* 0=Lie-Euler, 1=RKMK, 2=Crouch-Grossman */
    uint8_t enable_atmosphere : 1;  /* Enable atmospheric solver */
    uint8_t enable_hydrology : 1;   /* Enable hydrology solver */
    uint8_t enable_soil : 1;        /* Enable soil moisture solver */
    uint8_t _reserved : 5;          /* Reserved flags */
} SimulationConfig;

/* ========================================================================
 * STATE VIEW STRUCTURE (Non-Owning)
 * ======================================================================== */

/**
 * Canonical state VIEW into simulation memory.
 *
 * This struct does NOT own memory - it provides pointers into the
 * simulation's single contiguous memory block.
 *
 * Design principles:
 *   - Read-only access for external consumers
 *   - Deterministic binary layout for hashing
 *   - Versioned for backward compatibility
 */
typedef struct {
    /* Metadata */
    uint64_t timestamp;             /* Simulation time (microseconds) */
    uint32_t version;               /* State schema version */
    uint32_t num_entities;          /* Number of SE(3) entities */
    uint32_t num_scalar_values;     /* Total scalar field values */

    /* Data pointers (into simulation memory block) */
    se3_pose_t* poses;              /* SE(3) poses [num_entities] */
    float* scalar_fields;           /* Scalar values [num_scalar_values] */

    /* Precision tracking */
    uint8_t precision_mode;         /* Current precision mode */
    uint8_t _padding[3];            /* Alignment */

    /* Diagnostics */
    uint64_t state_hash;            /* XXH3 hash of full state */
    float energy;                   /* Total system energy (optional) */
    float max_error;                /* Maximum numerical error (optional) */
} SimulationState;

/* ========================================================================
 * STATE MANAGEMENT FUNCTIONS
 * ======================================================================== */

/**
 * Create a new simulation state from configuration.
 *
 * Allocates a single contiguous memory block and initializes all fields.
 *
 * @param cfg Configuration structure
 * @return Opaque simulation handle (NULL on failure)
 */
void* state_create(const SimulationConfig* cfg);

/**
 * Destroy simulation and free all memory.
 *
 * @param sim Opaque simulation handle
 */
void state_destroy(void* sim);

/**
 * Get a view into the current simulation state.
 *
 * The returned structure contains pointers into the simulation's memory.
 * Do NOT free or modify the pointed-to data.
 *
 * @param sim Opaque simulation handle
 * @param out_state Output state view (caller-allocated)
 * @return true on success, false on error
 */
bool state_get_view(void* sim, SimulationState* out_state);

/**
 * Advance simulation by one timestep.
 *
 * Pure function: takes current state, returns new state.
 * Integrator and solvers are selected based on SimulationConfig.
 *
 * @param sim Opaque simulation handle
 * @param dt Timestep (seconds, use 0 for config default)
 * @return true on success, false on numerical instability
 */
bool state_step(void* sim, float dt);

/**
 * Reset simulation to a specific binary state (for replay/debugging).
 *
 * @param sim Opaque simulation handle
 * @param buffer Binary state buffer (from neg_get_state_binary)
 * @param len Buffer length
 * @return true on success, false if buffer is invalid
 */
bool state_reset_from_binary(void* sim, const uint8_t* buffer, size_t len);

/**
 * Compute deterministic hash of current state.
 *
 * Uses XXH3 for speed. Hash is deterministic across platforms.
 *
 * @param sim Opaque simulation handle
 * @return 64-bit state hash
 */
uint64_t state_hash(void* sim);

/**
 * Serialize state to binary format (platform-independent).
 *
 * @param sim Opaque simulation handle
 * @param buffer Output buffer (caller-allocated)
 * @param max_len Maximum buffer size
 * @return Number of bytes written, or 0 on error
 */
size_t state_to_binary(void* sim, uint8_t* buffer, size_t max_len);

/**
 * Get required buffer size for binary serialization.
 *
 * @param sim Opaque simulation handle
 * @return Required buffer size in bytes
 */
size_t state_get_binary_size(void* sim);

#ifdef __cplusplus
}
#endif

#endif /* NE_STATE_H */
