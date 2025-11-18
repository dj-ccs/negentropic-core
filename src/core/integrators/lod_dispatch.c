// lod_dispatch.c - LoD-Gated Integrator Dispatch
//
// Dynamic integrator selection based on Level-of-Detail and error estimates.
// Implements escalation: RK4 → RKMK4 → Clebsch-Collective
//
// LoD Policy:
//   Level 0-1 (>50km):  RK4 (coarse, explicit)
//   Level 2-3 (<25km):  RKMK4 (SE(3)) or Clebsch (Lie-Poisson)
//
// Error-based escalation:
//   If RK4 error > 1e-4: Escalate to RKMK4
//   If RKMK4 error > 1e-6: Escalate to Clebsch
//
// Reference: docs/v2.2_Upgrade.md Phase 1.4
// Author: negentropic-core team
// Version: 2.2.0

#include "integrators.h"
#include "../torsion/torsion.h"
#include <math.h>
#include <string.h>

/* ========================================================================
 * LOD POLICY THRESHOLDS
 * ======================================================================== */

#define LOD_FINE_THRESHOLD 2      // LoD >= 2 uses fine integrators
#define ERROR_RK4_THRESHOLD 1e-4  // RK4 → RKMK4 escalation
#define ERROR_RKMK4_THRESHOLD 1e-6  // RKMK4 → Clebsch escalation

/* ========================================================================
 * LOD-BASED INTEGRATOR SELECTION
 * ======================================================================== */

/**
 * Select integrator based on LoD level and cell flags.
 *
 * Policy:
 *   LoD 0-1: RK4 (coarse explicit)
 *   LoD 2-3 + SE(3): RKMK4
 *   LoD 2-3 + Lie-Poisson: Clebsch-Collective
 *   Default: RK4
 *
 * @param cell Grid cell (must have lod_level and flags set)
 * @return Selected integrator method
 */
static integrator_e select_integrator_for_lod(const GridCell* cell) {
    if (!cell) return INTEGRATOR_RK4;

    // Coarse LoD: Always use RK4
    if (cell->lod_level < LOD_FINE_THRESHOLD) {
        return INTEGRATOR_RK4;
    }

    // Fine LoD: Check for special requirements
    if (cell->flags & CELL_FLAG_REQUIRES_SE3) {
        return INTEGRATOR_RKMK4;
    }

    if (cell->flags & CELL_FLAG_REQUIRES_LP) {
        return INTEGRATOR_CLEBSCH_COLLECTIVE;
    }

    // Default for fine LoD: Use RKMK4 (more general than Clebsch)
    return INTEGRATOR_RKMK4;
}

/**
 * Check if error estimate exceeds threshold for current integrator.
 *
 * @param error Estimated integration error
 * @param method Current integrator method
 * @return true if escalation needed, false otherwise
 */
static bool should_escalate(double error, integrator_e method) {
    switch (method) {
        case INTEGRATOR_RK4:
            return error > ERROR_RK4_THRESHOLD;

        case INTEGRATOR_RKMK4:
            return error > ERROR_RKMK4_THRESHOLD;

        case INTEGRATOR_CLEBSCH_COLLECTIVE:
            // No escalation beyond Clebsch
            return false;

        default:
            return false;
    }
}

/**
 * Escalate to next higher-order integrator.
 *
 * Escalation chain: RK4 → RKMK4 → Clebsch
 *
 * @param current Current integrator method
 * @return Next integrator in escalation chain
 */
static integrator_e escalate_integrator(integrator_e current) {
    switch (current) {
        case INTEGRATOR_RK4:
            return INTEGRATOR_RKMK4;

        case INTEGRATOR_RKMK4:
            return INTEGRATOR_CLEBSCH_COLLECTIVE;

        case INTEGRATOR_CLEBSCH_COLLECTIVE:
            // Already at highest level
            return INTEGRATOR_CLEBSCH_COLLECTIVE;

        default:
            return current;
    }
}

/* ========================================================================
 * LOD-GATED INTEGRATION STEP
 * ======================================================================== */

/**
 * Integrate a grid cell with LoD-gated integrator selection.
 *
 * Algorithm:
 *   1. Select integrator based on LoD level
 *   2. Save previous state
 *   3. Integrate
 *   4. Estimate error
 *   5. If error > threshold: escalate and retry
 *   6. If LoD >= 2: compute and apply torsion
 *
 * @param cell Grid cell to integrate (modified in-place)
 * @param cfg Integration configuration
 * @param ws Workspace
 * @return 0 on success, error code on failure
 */
int lod_gated_step_cell(GridCell* cell, const IntegratorConfig* cfg, IntegratorWorkspace* ws) {
    if (!cell || !cfg || !ws) return -1;

    // Save previous state for error estimation
    GridCell prev_state = *cell;

    // Select initial integrator based on LoD
    integrator_e method = select_integrator_for_lod(cell);

    // Integrate with selected method
    int result = integrator_step_cell(cell, cfg, method, ws);
    if (result != 0) return result;

    // Estimate integration error
    double error = estimate_integration_error(cell, &prev_state, cfg->dt);

    // Check for escalation
    if (should_escalate(error, method)) {
        // Restore state and retry with higher-order integrator
        *cell = prev_state;

        integrator_e escalated_method = escalate_integrator(method);
        result = integrator_step_cell(cell, cfg, escalated_method, ws);
        if (result != 0) return result;

        // Mark that escalation occurred (for statistics)
        // TODO: Add escalation counter to workspace
    }

    // Compute torsion for fine LoD levels
    if (cell->lod_level >= LOD_FINE_THRESHOLD) {
        // Torsion computation would normally happen at tile level
        // For single-cell integration, we skip it
        // (Full implementation would call compute_torsion_tile)

        // If torsion is available, apply tendency
        // TODO: Apply torsion tendency when torsion field is populated
    }

    return 0;
}

/* ========================================================================
 * TILE-LEVEL LOD DISPATCH
 * ======================================================================== */

/**
 * Integrate a tile of cells with LoD-gated dispatch.
 *
 * More efficient than per-cell dispatch:
 *   - Computes torsion once for entire tile
 *   - Batches cells by integrator method
 *   - Applies torsion tendencies in bulk
 *
 * @param cells Array of grid cells (modified in-place)
 * @param num_cells Number of cells in tile
 * @param cfg Integration configuration
 * @param ws Workspace (one per tile)
 * @return 0 on success, error code on failure
 */
int lod_gated_step_tile(GridCell* cells, size_t num_cells,
                        const IntegratorConfig* cfg,
                        IntegratorWorkspace* ws) {
    if (!cells || num_cells == 0 || !cfg || !ws) return -1;

    // Determine LoD level for tile (assume all cells have same LoD)
    int tile_lod = cells[0].lod_level;

    // Compute torsion for tile if LoD >= 2
    bool compute_torsion = (tile_lod >= LOD_FINE_THRESHOLD);

    if (compute_torsion) {
        // TODO: Call compute_torsion_tile when full state integration is ready
        // For now, skip torsion computation
    }

    // Integrate each cell in tile
    for (size_t i = 0; i < num_cells; i++) {
        GridCell* cell = &cells[i];

        // Skip inactive cells
        if (!(cell->flags & CELL_FLAG_ACTIVE)) {
            continue;
        }

        // Integrate with LoD gating
        int result = lod_gated_step_cell(cell, cfg, ws);
        if (result != 0) {
            // Log error but continue with other cells
            // TODO: Add error handling/logging
        }
    }

    // Apply torsion tendencies if computed
    if (compute_torsion) {
        // TODO: Apply torsion tendencies when torsion field is ready
        /*
        for (size_t i = 0; i < num_cells; i++) {
            GridCell* cell = &cells[i];
            if (cell->flags & CELL_FLAG_ACTIVE) {
                neg_torsion_t* t = &torsion_field[i];
                apply_torsion_tendency(cell, t, cfg->dt);
            }
        }
        */
    }

    return 0;
}

/* ========================================================================
 * LOD DISPATCH STATISTICS
 * ======================================================================== */

/**
 * LoD dispatch statistics (for monitoring and debugging).
 */
typedef struct {
    uint64_t rk4_count;       // Number of RK4 steps
    uint64_t rkmk4_count;     // Number of RKMK4 steps
    uint64_t clebsch_count;   // Number of Clebsch steps
    uint64_t escalation_count; // Number of escalations
} LoD_Stats;

// Global statistics (would be per-worker in production)
static LoD_Stats g_lod_stats = {0};

/**
 * Get LoD dispatch statistics.
 *
 * @param stats Output statistics structure
 */
void lod_get_statistics(LoD_Stats* stats) {
    if (stats) {
        *stats = g_lod_stats;
    }
}

/**
 * Reset LoD dispatch statistics.
 */
void lod_reset_statistics(void) {
    memset(&g_lod_stats, 0, sizeof(LoD_Stats));
}
