/*
 * negentropic.h - Public C API for negentropic-core
 *
 * Safe, minimal C API for all language bindings (Unity C#, WASM, Python).
 * Designed for:
 *   - Caller-allocated buffers (no hidden allocations)
 *   - Deterministic replay (via binary state snapshots)
 *   - Hash-based validation (detect silent corruption)
 *
 * Usage:
 *   1. Create simulation: neg_create(config_json)
 *   2. Step simulation: neg_step(sim, dt)
 *   3. Get state: neg_get_state_json() or neg_get_state_binary()
 *   4. Validate: neg_get_state_hash()
 *   5. Replay: neg_reset_from_binary() + neg_step() loop
 *   6. Destroy: neg_destroy(sim)
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#ifndef NE_API_H
#define NE_API_H

#include <stdint.h>
#include <stddef.h>
#include "../core/include/neg_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * VERSION INFORMATION
 * ======================================================================== */

#define NEGENTROPIC_VERSION_MAJOR 0
#define NEGENTROPIC_VERSION_MINOR 1
#define NEGENTROPIC_VERSION_PATCH 0

/**
 * Get version string (e.g., "0.1.0").
 *
 * @return Static version string
 */
const char* neg_get_version(void);

/* ========================================================================
 * LIFECYCLE MANAGEMENT
 * ======================================================================== */

/**
 * Create a new simulation from JSON configuration.
 *
 * Example JSON:
 * {
 *   "num_entities": 100,
 *   "num_scalar_fields": 10000,
 *   "grid_width": 100,
 *   "grid_height": 100,
 *   "grid_depth": 1,
 *   "dt": 0.016,
 *   "precision_mode": 1,
 *   "integrator_type": 0,
 *   "enable_atmosphere": true,
 *   "enable_hydrology": false,
 *   "enable_soil": true
 * }
 *
 * @param config_json Null-terminated JSON string
 * @return Opaque simulation handle (NULL on error)
 */
void* neg_create(const char* config_json);

/**
 * Destroy simulation and free all resources.
 *
 * @param sim Opaque simulation handle (NULL is safe)
 */
void neg_destroy(void* sim);

/* ========================================================================
 * SIMULATION CONTROL
 * ======================================================================== */

/**
 * Advance simulation by one timestep.
 *
 * @param sim Opaque simulation handle
 * @param dt Timestep in seconds (use 0.0 for config default)
 * @return 0 on success, negative error code on failure
 */
int neg_step(void* sim, float dt);

/**
 * Reset simulation to a specific binary state (for deterministic replay).
 *
 * Use case: Load a checkpoint and re-run simulation from that point.
 *
 * @param sim Opaque simulation handle
 * @param buffer Binary state buffer (from neg_get_state_binary)
 * @param len Buffer length
 * @return 0 on success, negative error code on failure
 */
int neg_reset_from_binary(void* sim, const uint8_t* buffer, size_t len);

/* ========================================================================
 * STATE RETRIEVAL (Safe, Caller-Allocated Buffers)
 * ======================================================================== */

/**
 * Get current state as binary blob (platform-independent).
 *
 * Binary format is deterministic and suitable for:
 *   - Hashing (reproducibility checks)
 *   - Network transmission
 *   - Replay/debugging
 *
 * @param sim Opaque simulation handle
 * @param buffer Caller-allocated buffer (use neg_get_state_binary_size first)
 * @param max_len Buffer size in bytes
 * @return Number of bytes written, or negative error code
 */
int neg_get_state_binary(void* sim, uint8_t* buffer, size_t max_len);

/**
 * Get required buffer size for binary state.
 *
 * Call this before neg_get_state_binary to allocate correct buffer size.
 *
 * @param sim Opaque simulation handle
 * @return Required buffer size in bytes, or 0 on error
 */
size_t neg_get_state_binary_size(void* sim);

/**
 * Get current state as JSON string.
 *
 * Format: {"timestamp": 123456, "poses": [...], "fields": [...]}
 *
 * @param sim Opaque simulation handle
 * @param buffer Caller-allocated buffer
 * @param max_len Buffer size in bytes
 * @return Number of bytes written (excluding null terminator), or negative error code
 */
int neg_get_state_json(void* sim, char* buffer, size_t max_len);

/* ========================================================================
 * VALIDATION & DIAGNOSTICS
 * ======================================================================== */

/**
 * Compute deterministic hash of current state.
 *
 * Critical for validation:
 *   - CI: Compare hash against Python oracle
 *   - Network: Detect silent corruption
 *   - Debugging: Verify deterministic execution
 *
 * Uses XXH3 (fast, portable, well-tested).
 *
 * @param sim Opaque simulation handle
 * @return 64-bit state hash
 */
uint64_t neg_get_state_hash(void* sim);

/**
 * Get last error message (if any).
 *
 * @return Static error string, or NULL if no error
 */
const char* neg_get_last_error(void);

/**
 * Get numerical error flags from simulation.
 *
 * Retrieves accumulated error flags that indicate numerical issues
 * such as overflow, underflow, NaN, integration failures, etc.
 *
 * @param sim Opaque simulation handle
 * @return Error flags structure (check error_flags.total_errors for count)
 */
NegErrorFlags neg_get_error_flags(void* sim);

/**
 * Get diagnostic metrics (JSON format).
 *
 * Example output:
 * {
 *   "energy": 1234.5,
 *   "max_error": 0.0001,
 *   "step_count": 1000,
 *   "so3_drift": 1.2e-8
 * }
 *
 * @param sim Opaque simulation handle
 * @param buffer Caller-allocated buffer
 * @param max_len Buffer size in bytes
 * @return Number of bytes written (excluding null terminator), or negative error code
 */
int neg_get_diagnostics(void* sim, char* buffer, size_t max_len);

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define NEG_SUCCESS                 0
#define NEG_ERROR_NULL_HANDLE      -1
#define NEG_ERROR_INVALID_CONFIG   -2
#define NEG_ERROR_BUFFER_TOO_SMALL -3
#define NEG_ERROR_NUMERICAL_INSTABILITY -4
#define NEG_ERROR_INVALID_STATE    -5
#define NEG_ERROR_OUT_OF_MEMORY    -6

#ifdef __cplusplus
}
#endif

#endif /* NE_API_H */
