/*
 * t_bsp.h - Trajectory Binary Space Partitioning (T-BSP)
 *
 * Doom-inspired spatial partitioning adapted for maritime vessel tracking.
 * Maps Doom's BSP tree traversal to fixed-grid lat/lon cells for edge computation.
 *
 * Doom Lineage:
 *   - node_t (BSP split plane) → t_bsp_cell_t (lat/lon grid cell)
 *   - R_RenderBSPNode() → t_bsp_insert_pose() (deterministic insertion)
 *   - subsector_t (leaf) → full cell triggers λ-computation
 *
 * Hardware Target: ESP32-S3 (512KB SRAM, no dynamic allocation)
 * Author: Grok (T-BSP design) + ClaudeCode (integration)
 * Version: 1.0
 */

#ifndef T_BSP_H
#define T_BSP_H

#include "se3_edge.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONFIGURATION CONSTANTS
 * ======================================================================== */

/**
 * Maximum poses per cell (ring buffer size).
 *
 * When cell fills (pose_count == MAX_POSES_PER_CELL):
 *   1. Trigger λ-estimation on full trajectory
 *   2. Publish DLT record to IOTA
 *   3. Reset cell for next voyage segment
 *
 * Memory: 128 poses × 56 bytes = 7,168 bytes per cell
 */
#define MAX_POSES_PER_CELL   128

/**
 * Maximum concurrent active cells.
 *
 * Typical usage:
 *   - Single vessel: 1-2 cells (current + handoff target)
 *   - Multi-vessel edge node: 10-20 cells
 *   - Port aggregator: 50-64 cells
 *
 * Memory: 64 cells × 7,192 bytes = ~460 KB SRAM
 */
#define MAX_CELLS            64

/**
 * Cell grid size (kilometers).
 *
 * Adaptive levels (future):
 *   - Level 0: 100 km (open ocean)
 *   - Level 1: 10 km (coastal - default)
 *   - Level 2: 1 km (ports/harbors)
 *
 * Note: This is a design-time constant, not runtime-configurable.
 */
#define CELL_SIZE_KM         10

/**
 * Approximate kilometers per degree latitude (WGS84).
 *
 * Units: (111.32 km/degree) * FRACUNIT (65,536)
 * Result: 7,292,723 in fixed-point = ~111.32 km
 *
 * Longitude varies with latitude (cos scaling), but for T-BSP
 * grid indexing we use this constant approximation at equator.
 */
#define FIXED_DEG_TO_KM      ((fixed_t)(111.32f * FRACUNIT))

/* Compile-time safety checks */
_Static_assert(MAX_CELLS <= 65536, "cell_id is uint16_t, MAX_CELLS must fit");
_Static_assert(MAX_POSES_PER_CELL > 0, "Must allow at least one pose per cell");

/* ========================================================================
 * DATA STRUCTURES
 * ======================================================================== */

/**
 * T-BSP cell: spatial partition for trajectory segments.
 *
 * Doom BSP mapping:
 *   - Doom node_t.bbox → t_bsp_cell_t.{lat,lon}_{min,max}
 *   - Doom subsector_t.sector → t_bsp_cell_t.poses[] (trajectory data)
 *   - Doom seg_t (line segment) → se3_pose_t (6-DOF pose)
 *
 * Memory layout: 7,192 bytes per cell
 *   - Bounds: 16 bytes
 *   - Metadata: 8 bytes
 *   - Poses: 128 × 56 = 7,168 bytes
 */
typedef struct {
    fixed_t lat_min, lat_max;   /**< Cell bounds in fixed-point degrees (WGS84) */
    fixed_t lon_min, lon_max;   /**< Normalized to [-180°, 180°] */
    uint16_t cell_id;            /**< Unique identifier (grid index encoded) */
    uint16_t pose_count;         /**< Current number of poses (0 to MAX_POSES_PER_CELL) */
    bool active;                 /**< Cell in use (false = available for allocation) */
    uint8_t _padding[3];         /**< Alignment padding (total 24 bytes metadata) */
    se3_pose_t poses[MAX_POSES_PER_CELL];  /**< Fixed-size trajectory buffer */
} t_bsp_cell_t;

/**
 * T-BSP root structure: manages all active cells.
 *
 * Doom BSP mapping:
 *   - Doom BSP tree root → t_bsp_t (global cell manager)
 *   - Doom numnodes → t_bsp_t.active_count
 *   - Static allocation (no malloc, ESP32-safe)
 */
typedef struct {
    t_bsp_cell_t cells[MAX_CELLS];  /**< Static cell array (~460 KB) */
    uint16_t active_count;           /**< Number of cells in use */
    uint16_t _padding;               /**< Alignment */
    fixed_t ref_lat, ref_lon;        /**< Voyage origin (grid reference point) */
} t_bsp_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * Initialize T-BSP root structure.
 *
 * Sets voyage origin (reference point for grid indexing) and
 * zeroes all cell data. Must be called before any other T-BSP functions.
 *
 * @param bsp T-BSP root structure (must be allocated by caller)
 * @param lat0 Reference latitude in fixed-point degrees (voyage start)
 * @param lon0 Reference longitude in fixed-point degrees (normalized to [-180, 180])
 */
void t_bsp_init(t_bsp_t* bsp, fixed_t lat0, fixed_t lon0);

/**
 * Convert lat/lon to cell ID (grid index).
 *
 * Doom BSP analog: R_PointInSubsector() → finds leaf node for XY point
 *
 * Algorithm:
 *   1. Normalize longitude to [-180°, 180°] (Doom wraparound)
 *   2. Compute offset from reference point (lat0, lon0)
 *   3. Divide by CELL_SIZE_KM to get grid indices
 *   4. Encode (lat_idx, lon_idx) into uint16_t cell_id
 *
 * Cell ID encoding: (lat_idx & 0xFF) << 8 | (lon_idx & 0xFF)
 *   - Supports ±127 cells from origin (±1,270 km at 10 km cell size)
 *
 * @param bsp T-BSP root structure
 * @param lat Vessel latitude (fixed-point degrees)
 * @param lon Vessel longitude (fixed-point degrees, auto-normalized)
 * @return cell_id for this position
 */
uint16_t t_bsp_latlon_to_cell(t_bsp_t* bsp, fixed_t lat, fixed_t lon);

/**
 * Insert pose into specified cell.
 *
 * Doom BSP analog: R_AddLine() → adds seg_t to subsector
 *
 * Behavior:
 *   - If cell doesn't exist, allocates from cells[] array
 *   - If cell full (pose_count == MAX_POSES_PER_CELL), triggers λ-estimation
 *     and resets cell (handled by caller via overflow flag)
 *   - Returns false only if MAX_CELLS exceeded (allocation failure)
 *
 * Performance: ~175 ns @ 240 MHz (42 cycles typical)
 *
 * @param bsp T-BSP root structure
 * @param cell_id Target cell (from t_bsp_latlon_to_cell)
 * @param pose SE(3) pose to insert (copied into cell)
 * @return true on success, false if MAX_CELLS exceeded
 */
bool t_bsp_insert_pose(t_bsp_t* bsp, uint16_t cell_id, const se3_pose_t* pose);

/**
 * Get cell by ID (read-only access).
 *
 * @param bsp T-BSP root structure
 * @param cell_id Cell identifier
 * @return Pointer to cell, or NULL if not found
 */
t_bsp_cell_t* t_bsp_get_cell(t_bsp_t* bsp, uint16_t cell_id);

/**
 * Reset cell for reuse (after λ-estimation).
 *
 * Clears pose_count and deactivates cell. Does not zero memory
 * (optimization: poses will be overwritten on next insert).
 *
 * @param bsp T-BSP root structure
 * @param cell_id Cell to reset
 */
void t_bsp_reset_cell(t_bsp_t* bsp, uint16_t cell_id);

/**
 * Get adjacent cell IDs (8-connectivity grid).
 *
 * For handoff prediction and multi-cell λ-estimation.
 * Returns up to 8 neighbors (fewer at grid boundaries).
 *
 * @param bsp T-BSP root structure
 * @param cell_id Center cell
 * @param neighbors Output array (must hold 8 uint16_t)
 * @param count Output: number of neighbors found (0-8)
 */
void t_bsp_get_adjacent_cells(t_bsp_t* bsp, uint16_t cell_id,
                              uint16_t* neighbors, int* count);

/**
 * Check if cell is near overflow (trigger preemptive λ-estimation).
 *
 * Useful for predictive computation before hard limit.
 *
 * @param cell Cell to check
 * @param threshold Fraction of MAX_POSES_PER_CELL (e.g., 0.9 for 90%)
 * @return true if pose_count >= threshold * MAX_POSES_PER_CELL
 */
bool t_bsp_cell_near_full(const t_bsp_cell_t* cell, float threshold);

/**
 * Get number of active cells (diagnostic function).
 *
 * @param bsp T-BSP root structure
 * @return Count of cells with active=true
 */
uint16_t t_bsp_get_active_count(const t_bsp_t* bsp);

/**
 * Compute cell bounds (lat/lon min/max) from cell ID.
 *
 * Useful for visualization and debugging.
 *
 * @param bsp T-BSP root (for reference point)
 * @param cell_id Cell identifier
 * @param lat_min Output: minimum latitude (fixed-point degrees)
 * @param lat_max Output: maximum latitude
 * @param lon_min Output: minimum longitude
 * @param lon_max Output: maximum longitude
 */
void t_bsp_get_cell_bounds(const t_bsp_t* bsp, uint16_t cell_id,
                           fixed_t* lat_min, fixed_t* lat_max,
                           fixed_t* lon_min, fixed_t* lon_max);

#ifdef __cplusplus
}
#endif

#endif /* T_BSP_H */
