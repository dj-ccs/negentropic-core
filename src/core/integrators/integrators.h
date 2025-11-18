// integrators.h - Structure-Preserving Integrators API
//
// Master API for negentropic-core v2.2 integrator stack:
//   - INTEGRATOR_RK4: Classic 4th-order Runge-Kutta (coarse LoD)
//   - INTEGRATOR_RKMK4: Runge-Kutta-Munthe-Kaas for SE(3) (fine LoD)
//   - INTEGRATOR_CLEBSCH_COLLECTIVE: Lie-Poisson symplectic (fine LoD)
//
// Reference: docs/integrators.md section 4.1
// Author: negentropic-core team
// Version: 2.2.0

#ifndef NEG_INTEGRATORS_H
#define NEG_INTEGRATORS_H

#include <stdint.h>
#include "../state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * INTEGRATOR ENUMERATION
 * ======================================================================== */

/**
 * Available integrator methods.
 *
 * Selection criteria:
 *   - RK4: LoD 0-1 (coarse grids, >50km spacing)
 *   - SYMPLECTIC_PRK: Explicit symplectic (fallback for Clebsch)
 *   - RKMK4: LoD 2-3 with SE(3) rotational dynamics
 *   - CLEBSCH_COLLECTIVE: LoD 2-3 with vorticity/Lie-Poisson dynamics
 *   - EXPLICIT_EULER: Debugging/testing only
 */
typedef enum {
    INTEGRATOR_RK4 = 0,                  // Classic 4th-order Runge-Kutta
    INTEGRATOR_SYMPLECTIC_PRK = 1,       // Partitioned Runge-Kutta (symplectic)
    INTEGRATOR_RKMK4 = 2,                // Runge-Kutta-Munthe-Kaas (SE(3))
    INTEGRATOR_CLEBSCH_COLLECTIVE = 3,   // Clebsch + symplectic (Lie-Poisson)
    INTEGRATOR_EXPLICIT_EULER = 4        // 1st-order (testing only)
} integrator_e;

/* ========================================================================
 * INTEGRATOR CONFIGURATION
 * ======================================================================== */

/**
 * Integrator configuration parameters.
 *
 * Controls convergence, accuracy, and fallback behavior.
 */
typedef struct {
    double dt;           // Timestep (seconds)
    int max_iter;        // Maximum iterations for implicit solves (default: 4)
    double tol;          // Convergence tolerance (default: 1e-6)
    uint32_t flags;      // Configuration flags (see below)
} IntegratorConfig;

/* Configuration flags */
#define INTEGRATOR_FLAG_PRESERVE_CASIMIRS   (1 << 0)  // Enforce Casimir conservation
#define INTEGRATOR_FLAG_ALLOW_APPROX        (1 << 1)  // Allow approximate solutions on timeout
#define INTEGRATOR_FLAG_TRACK_ERROR         (1 << 2)  // Compute and store error estimates
#define INTEGRATOR_FLAG_USE_LUT_ACCEL       (1 << 3)  // Use LUT acceleration (default: on)

/* ========================================================================
 * INTEGRATOR WORKSPACE
 * ======================================================================== */

/**
 * Workspace for integrator scratch space.
 *
 * Allocated once per worker thread to avoid dynamic allocation
 * in the integration loop. Reused across multiple steps.
 *
 * Size: ~1-2 KB per workspace
 */
typedef struct IntegratorWorkspace IntegratorWorkspace;

/**
 * Create integrator workspace.
 *
 * @param max_dim Maximum dimension for state vectors (typically 8-12)
 * @return Workspace pointer, or NULL on allocation failure
 */
IntegratorWorkspace* integrator_workspace_create(size_t max_dim);

/**
 * Destroy integrator workspace and free memory.
 *
 * @param ws Workspace to destroy (can be NULL)
 */
void integrator_workspace_destroy(IntegratorWorkspace* ws);

/**
 * Reset workspace to initial state.
 *
 * Clears temporary buffers without freeing memory.
 * Use between integration steps if needed.
 *
 * @param ws Workspace to reset (must be valid)
 */
void integrator_workspace_reset(IntegratorWorkspace* ws);

/* ========================================================================
 * GRID CELL STRUCTURE
 * ======================================================================== */

/**
 * Grid cell state (simplified for v2.2).
 *
 * Contains all state variables for a single grid cell.
 * Layout matches SAB scalar field offsets.
 */
typedef struct {
    // Hydrology
    float theta;              // Soil moisture (volumetric fraction)
    float surface_water;      // Surface water depth (mm)

    // Soil
    float SOM;                // Soil organic matter (%)
    float temperature;        // Soil temperature (°C)

    // Vegetation
    float vegetation;         // Vegetation cover (fraction)

    // Momentum (for torsion coupling)
    float momentum_u;         // East-west momentum
    float momentum_v;         // North-south momentum

    // Vorticity (Lie-Poisson variable)
    float vorticity;          // Vertical component (for Clebsch)

    // Metadata
    uint32_t flags;           // Cell flags (active, boundary, etc.)
    int lod_level;            // Current LoD level (0-3)
} GridCell;

/* Cell flags */
#define CELL_FLAG_ACTIVE        (1 << 0)  // Cell is active (not culled)
#define CELL_FLAG_BOUNDARY      (1 << 1)  // Cell is at domain boundary
#define CELL_FLAG_REQUIRES_SE3  (1 << 2)  // Cell requires SE(3) integration
#define CELL_FLAG_REQUIRES_LP   (1 << 3)  // Cell requires Lie-Poisson integration

/* ========================================================================
 * MAIN INTEGRATION API
 * ======================================================================== */

/**
 * Integrate a single grid cell forward by dt.
 *
 * Automatically selects integrator based on method parameter.
 * Updates cell state in-place.
 *
 * @param cell Grid cell to integrate (must be valid, non-NULL)
 * @param cfg Integration configuration (must be valid, non-NULL)
 * @param method Integrator method to use
 * @param ws Workspace for scratch space (must be valid, non-NULL)
 * @return 0 on success, non-zero error code on failure
 *
 * Error codes:
 *   -1: Invalid parameters
 *   -2: Integration diverged (error > tolerance after max_iter)
 *   -3: Numerical instability detected
 *   -4: Unsupported integrator method
 */
int integrator_step_cell(GridCell* cell,
                         const IntegratorConfig* cfg,
                         integrator_e method,
                         IntegratorWorkspace* ws);

/* ========================================================================
 * ERROR ESTIMATION
 * ======================================================================== */

/**
 * Estimate integration error for a cell.
 *
 * Used for dynamic LoD escalation:
 *   - If error > threshold_1: upgrade RK4 → RKMK4
 *   - If error > threshold_2: upgrade RKMK4 → Clebsch
 *
 * @param cell Grid cell (must be valid)
 * @param prev_state Previous state (for comparison)
 * @param dt Timestep used
 * @return Estimated error magnitude
 */
double estimate_integration_error(const GridCell* cell,
                                   const GridCell* prev_state,
                                   double dt);

/* ========================================================================
 * INITIALIZATION & CONFIGURATION
 * ======================================================================== */

/**
 * Initialize integrator subsystem.
 *
 * Must be called once before using integrators.
 * Initializes LUT tables and precomputes coefficients.
 */
void integrator_init(void);

/**
 * Initialize integrator configuration with default values.
 *
 * @param cfg Configuration structure to initialize (must be valid)
 */
void integrator_config_init(IntegratorConfig* cfg);

/**
 * Set default timestep for integrator configuration.
 *
 * @param cfg Configuration to modify (must be valid)
 * @param dt Timestep in seconds (must be > 0)
 */
void integrator_config_set_dt(IntegratorConfig* cfg, double dt);

/**
 * Enable/disable Casimir preservation.
 *
 * @param cfg Configuration to modify (must be valid)
 * @param enable true to enforce Casimirs, false to allow drift
 */
void integrator_config_set_preserve_casimirs(IntegratorConfig* cfg, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* NEG_INTEGRATORS_H */
