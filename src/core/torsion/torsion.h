// torsion.h - Torsion Tensor API for negentropic-core v2.2
//
// Discrete curl operator and momentum coupling for 2.5D vorticity simulation.
// Implements CliMA-style weak curl with cubed-sphere edge continuity.
//
// Reference: docs/integrators.md section 4.2
// Author: negentropic-core team
// Version: 2.2.0

#ifndef NEG_TORSION_H
#define NEG_TORSION_H

#include "../state.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * TORSION DATA STRUCTURE
 * ======================================================================== */

/**
 * Torsion (vorticity) vector per cell.
 *
 * Layout: 16 bytes per cell (4 × fixed_t)
 * - wx, wy, wz: Components of discrete curl(velocity)
 * - mag: Magnitude for fast shader access
 *
 * Memory: Stored in SAB at offset_torsion (from header)
 */
typedef struct {
    float wx;   // X-component of vorticity (Q16.16 in SAB, float for CPU)
    float wy;   // Y-component
    float wz;   // Z-component (primary for 2.5D)
    float mag;  // Magnitude: sqrt(wx² + wy² + wz²)
} neg_torsion_t;

/* ========================================================================
 * TORSION COMPUTATION API
 * ======================================================================== */

/**
 * Compute torsion for a rectangular tile of cells.
 *
 * Discrete curl operator using CliMA weak-form curl:
 *   ω = ∇ × u (discrete)
 *
 * For 2.5D simulation:
 *   ωz = ∂v/∂x - ∂u/∂y (horizontal vorticity)
 *
 * Stencil: 5-point central differences (interior cells)
 *          One-sided differences (boundary cells)
 *
 * Writes torsion into SimulationState.sab_torsion field (if present).
 *
 * @param S Simulation state (must be valid pointer)
 * @param x0 Starting x-coordinate in grid
 * @param y0 Starting y-coordinate in grid
 * @param nx Number of cells in x-direction (tile width)
 * @param ny Number of cells in y-direction (tile height)
 * @return 0 on success, non-zero error code on failure
 */
int compute_torsion_tile(SimulationState* S, size_t x0, size_t y0, size_t nx, size_t ny);

/**
 * Apply torsion tendencies to momentum fields.
 *
 * Conservative momentum increment:
 *   Δu ∝ α × ω × dt
 *
 * where α is the LoD-scaled coupling coefficient:
 *   alpha = 8e-4 * (lod_level / 3.0)^1.5
 *
 * Physics interpretation:
 *   - Vorticity drives secondary circulation
 *   - Small amplitude correction (preserves stability)
 *   - LoD scaling: minimal at coarse grids, strong at fine grids
 *   - Power 1.5: Super-linear enhancement for high-resolution dynamics
 *
 * @param cell Grid cell to update (must be valid pointer, must have lod_level set)
 * @param t Torsion vector (must be valid pointer)
 * @param dt Timestep in seconds
 */
void apply_torsion_tendency(GridCell* cell, const neg_torsion_t* t, double dt);

/**
 * Compute torsion magnitude from components.
 *
 * Fast magnitude calculation:
 *   |ω| = sqrt(wx² + wy² + wz²)
 *
 * @param t Torsion vector (must be valid pointer)
 * @return Magnitude (always non-negative)
 */
float compute_torsion_magnitude(const neg_torsion_t* t);

/* ========================================================================
 * CLOUD SEEDING COUPLING
 * ======================================================================== */

/**
 * Enhance cloud seeding probability based on torsion magnitude.
 *
 * Cloud seeding enhancement:
 *   p_cloud' = p_cloud + κ × |ω|
 *
 * where κ is the torsion-cloud coupling coefficient (default: 0.1).
 *
 * Physics interpretation:
 *   - High vorticity → enhanced vertical motion
 *   - Vertical motion → cloud formation
 *   - Creates visual correlation between dynamics and clouds
 *
 * @param base_probability Base cloud probability [0, 1]
 * @param torsion_mag Torsion magnitude (from compute_torsion_magnitude)
 * @param coupling_coeff Coupling coefficient κ (typically 0.1)
 * @return Enhanced probability [0, 1] (clamped)
 */
float enhance_cloud_probability(float base_probability, float torsion_mag, float coupling_coeff);

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

/**
 * Torsion computation configuration.
 *
 * Allows runtime tuning of coupling coefficients and thresholds.
 */
typedef struct {
    float momentum_coupling_alpha;  // Momentum tendency coefficient (default: 1e-3)
    float cloud_coupling_kappa;     // Cloud seeding coefficient (default: 0.1)
    float min_magnitude_threshold;  // Ignore torsion below this magnitude (default: 1e-6)
    bool enable_momentum_coupling;  // Enable/disable momentum tendencies (default: true)
    bool enable_cloud_coupling;     // Enable/disable cloud seeding (default: true)
} neg_torsion_config_t;

/**
 * Initialize torsion configuration with default values.
 *
 * @param cfg Configuration structure to initialize (must be valid pointer)
 */
void neg_torsion_config_init(neg_torsion_config_t* cfg);

/* ========================================================================
 * DIAGNOSTICS
 * ======================================================================== */

/**
 * Compute global torsion statistics for a simulation state.
 *
 * Useful for monitoring and validation.
 *
 * @param S Simulation state (must be valid pointer)
 * @param mean_mag Output: Mean torsion magnitude (can be NULL)
 * @param max_mag Output: Maximum torsion magnitude (can be NULL)
 * @param total_enstrophy Output: Total enstrophy (∫|ω|² dA) (can be NULL)
 * @return 0 on success, non-zero on error
 */
int compute_torsion_statistics(const SimulationState* S,
                                float* mean_mag,
                                float* max_mag,
                                float* total_enstrophy);

#ifdef __cplusplus
}
#endif

#endif /* NEG_TORSION_H */
