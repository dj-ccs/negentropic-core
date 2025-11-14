/*
 * regeneration_cascade.h - Regeneration Cascade Solver (REGv1)
 *
 * Implements the slow-timescale vegetation-SOM-moisture feedback loop that
 * models ecosystem-wide phase transitions from degraded to regenerative states.
 *
 * Scientific Foundation:
 *   This solver implements Edison's "V-SOM-θ Coupling" framework for the
 *   Loess Plateau and arid/semi-arid ecosystems, with Grok's high-performance
 *   fixed-point numerical scheme for embedded deployment.
 *
 * Core Principle:
 *   Vegetation (V), Soil Organic Matter (SOM), and moisture (θ) form a positive
 *   feedback loop that drives state transitions. Once critical thresholds are
 *   exceeded (θ > θ*, SOM > SOM*), the system "wakes up" and transitions from
 *   degraded to regenerative states along a sigmoid trajectory.
 *
 * Key Equations (from Edison Research Specification):
 *
 *   [Vegetation ODE]
 *       dV/dt = r_V * V * (1 - V/K_V)
 *               + λ1 * max(θ_avg - θ*, 0)
 *               + λ2 * max(SOM - SOM*, 0)
 *
 *   [SOM ODE]
 *       dSOM/dt = a1 * V - a2 * SOM
 *
 *   [Hydrological Bonus] Coupling back to HYD-RLv1:
 *       porosity_eff += η1 * dSOM  (η1 = 5 mm per %SOM)
 *       K_zz *= (1.15)^dSOM        (15% compounding increase per %SOM)
 *
 * Performance Constraints (Non-Negotiable):
 *   - Execution frequency: Once every 128 hydrology steps
 *   - Performance budget: < 5% of total frame time
 *   - Data types: Fixed-point 16.16 arithmetic for V and SOM
 *
 * Coupling to HYD-RLv1:
 *   This solver MODIFIES the following Cell state variables that are READ
 *   by the HYD-RLv1 hydrology_step():
 *     - porosity_eff: water-holding capacity (adds η1 * dSOM)
 *     - K_tensor[8]: vertical conductivity (multiplies by 1.15^dSOM)
 *
 *   The coupling is one-way per step: REGv1 runs, modifies Cell state,
 *   then HYD reads the updated values for the next 128 steps.
 *
 * Implementation Notes:
 *   - Pure function design: stateless, thread-safe
 *   - Fixed-point arithmetic: 16.16 format for V and SOM calculations
 *   - Performance optimized: minimal per-cell operations
 *   - Physically consistent: Mass-conserving, threshold-respecting
 *
 * Design Principles:
 *   - Stateless: All data passed explicitly via Cell structs
 *   - Side-effect-free: No global state (except parameter loading)
 *   - Thread-safe: Can be parallelized
 *   - Physically consistent: Threshold-driven phase transitions
 *
 * References:
 *   See docs/science/macroscale_regeneration.md for complete references
 *   and calibration anchors from Loess Plateau (1995-2010).
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (REGv1)
 * License: MIT OR GPL-3.0
 */

#ifndef REGENERATION_CASCADE_H
#define REGENERATION_CASCADE_H

#include <stddef.h>
#include <stdint.h>
#include "hydrology_richards_lite.h"  /* For Cell struct definition */

/* ========================================================================
 * CORE DATA STRUCTURES
 * ======================================================================== */

/**
 * RegenerationParams - Parameters for the regeneration cascade solver.
 *
 * These parameters control the vegetation-SOM-moisture feedback dynamics.
 * They are loaded from JSON files (LoessPlateau.json, ChihuahuanDesert.json)
 * at initialization and remain constant during simulation.
 *
 * CRITICAL: Parameters must be pre-loaded into this flat C struct at
 * initialization. NO runtime JSON parsing allowed.
 */
typedef struct {
    /* ---- Vegetation Dynamics ---- */
    float r_V;              /* Vegetation intrinsic growth rate [yr^-1]
                             * Range: [0.06, 0.20]
                             * Loess Plateau: 0.12 yr^-1
                             * Chihuahuan Desert: 0.06 yr^-1 */

    float K_V;              /* Vegetation carrying capacity (max cover) [-]
                             * Range: [0.45, 0.80]
                             * Loess Plateau: 0.70
                             * Chihuahuan Desert: 0.45 */

    float lambda1;          /* Moisture-to-vegetation coupling [yr^-1 per (m^3/m^3)]
                             * Range: [0.20, 0.90]
                             * Controls: V response to θ > θ* */

    float lambda2;          /* SOM-to-vegetation coupling [yr^-1 per %SOM]
                             * Range: [0.03, 0.25]
                             * Controls: V response to SOM > SOM* */

    /* ---- Critical Thresholds ---- */
    float theta_star;       /* Critical soil moisture threshold [m^3/m^3]
                             * Range: [0.10, 0.20]
                             * Below this: no moisture-driven V growth */

    float SOM_star;         /* Critical SOM threshold [%]
                             * Range: [0.7, 1.8]
                             * Below this: no SOM-driven V growth */

    /* ---- SOM Dynamics ---- */
    float a1;               /* SOM input rate per V [% SOM yr^-1 per V]
                             * Range: [0.05, 0.30]
                             * Loess Plateau: 0.18 */

    float a2;               /* SOM decay rate [yr^-1]
                             * Range: [0.02, 0.06]
                             * Temperature modulation optional */

    /* ---- Hydrological Coupling ---- */
    float eta1;             /* Water holding capacity gain [mm per %SOM]
                             * Range: [2.0, 12.0]
                             * Physical basis: +1% SOM → +η1 mm WHC
                             * Loess Plateau: 5.0 mm/%SOM */

    float K_vertical_multiplier; /* K_zz multiplier per %SOM [-]
                                  * Value: 1.15 (15% compounding increase)
                                  * Formula: K_zz *= (1.15)^dSOM */

} RegenerationParams;

/* ========================================================================
 * SOLVER FUNCTIONS
 * ======================================================================== */

/**
 * Load regeneration parameters from JSON file.
 *
 * Parses a JSON parameter file and populates the RegenerationParams struct.
 * This function must be called once at initialization before any simulation.
 *
 * Supported parameter sets:
 *   - LoessPlateau.json: Calibrated to Loess Plateau (1995-2010)
 *   - ChihuahuanDesert.json: Calibrated to arid/semi-arid desert systems
 *
 * @param filename      Path to JSON parameter file
 * @param params        [OUT] Parameter struct to populate
 * @return 0 on success, -1 on error
 *
 * Performance: This function performs file I/O and JSON parsing. It is NOT
 *              performance-critical and should only be called at initialization.
 */
int regeneration_cascade_load_params(const char* filename, RegenerationParams* params);

/**
 * Load REGv2 microbial parameters and enable enhanced SOM dynamics.
 *
 * Call this function at initialization to enable the REGv2 microbial priming
 * module. If not called, REGv1 will use the simple linear SOM model.
 *
 * When enabled, SOM dynamics use:
 *   - Fungal-bacterial priming (8-entry lookup table with hard anchors)
 *   - Monod-Arrhenius kinetics for temperature and moisture
 *   - Aggregate stability enhancement
 *   - Explosive recovery when F:B > 1.0 (Johnson-Su effect)
 *
 * @param filename      Path to REGv2_Microbial.json parameter file
 * @return 0 on success, -1 on error
 */
int regeneration_cascade_enable_regv2(const char* filename);

/**
 * Advance regeneration state by one timestep.
 *
 * Implements the slow-timescale vegetation-SOM-moisture feedback dynamics
 * using Grok's fixed-point numerical scheme:
 *   1. Read current state (V, SOM, θ_avg) from Cell
 *   2. Solve coupled ODEs with threshold activation
 *   3. Update state with clamping to valid ranges
 *   4. Apply hydrological bonus to porosity_eff and K_tensor
 *
 * Fixed-Point Strategy:
 *   - V and SOM calculations use 16.16 fixed-point (int32_t)
 *   - Convert to float for ODEs, back to fixed-point for storage
 *   - Performance target: < 5% of frame budget
 *
 * Hydrological Bonus (THE CRITICAL COUPLING):
 *   This function MODIFIES Cell fields that HYD-RLv1 READS:
 *     - porosity_eff: adds η1 * dSOM converted to [m³/m³]
 *     - K_tensor[8]: multiplies by (1.15)^dSOM (compounding)
 *
 *   Physical meaning:
 *     - +1% SOM → +5mm water holding (via porosity increase)
 *     - +1% SOM → +15% vertical conductivity (via macropore formation)
 *
 * Execution Frequency:
 *   Call this function once every 128 hydrology steps. The slow timescale
 *   (years) means high-frequency calls waste computation.
 *
 * @param grid          [IN/OUT] Array of Cell structs (spatial grid)
 * @param grid_size     Number of cells in grid
 * @param params        [IN] Pre-loaded regeneration parameters
 * @param dt_years      Timestep [years] (typically 1.0 year)
 *
 * Thread Safety: Pure function, can be called concurrently with different
 *                Cell arrays.
 *
 * Performance: Target ~20-30 ns/cell with fixed-point optimizations.
 */
void regeneration_cascade_step(
    Cell* grid,
    size_t grid_size,
    const RegenerationParams* params,
    float dt_years
);

/* ========================================================================
 * DIAGNOSTIC FUNCTIONS
 * ======================================================================== */

/**
 * Check if cell has crossed critical thresholds (diagnostic).
 *
 * Returns a bitmask indicating which thresholds have been exceeded:
 *   Bit 0: θ > θ* (moisture above critical)
 *   Bit 1: SOM > SOM* (organic matter above critical)
 *   Bit 2: V > 0.5*K_V (vegetation above midpoint)
 *
 * Used for validation and visualization of phase transitions.
 *
 * @param cell          Cell to diagnose
 * @param params        Parameter set (for thresholds)
 * @return Bitmask of exceeded thresholds
 */
int regeneration_cascade_threshold_status(
    const Cell* cell,
    const RegenerationParams* params
);

/**
 * Compute total ecosystem state score (diagnostic).
 *
 * Returns a single scalar "health" metric combining V, SOM, and θ:
 *   Score = w_V * V + w_SOM * (SOM/SOM_max) + w_θ * (θ/θ_max)
 *
 * Range: [0, 1] where:
 *   - 0.0: Degraded (bare, no SOM, dry)
 *   - 1.0: Regenerated (full vegetation, high SOM, moist)
 *
 * Used for validation tests and visualization.
 *
 * @param cell          Cell to score
 * @param params        Parameter set
 * @return Health score [0, 1]
 */
float regeneration_cascade_health_score(
    const Cell* cell,
    const RegenerationParams* params
);

#endif /* REGENERATION_CASCADE_H */
