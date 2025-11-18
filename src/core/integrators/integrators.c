// integrators.c - Main Integrator Dispatcher
//
// Routes integration requests to appropriate integrator based on method.
// Implements error estimation for dynamic LoD escalation.
//
// Author: negentropic-core team
// Version: 2.2.0

#include "integrators.h"
#include "workspace.h"
#include <math.h>
#include <string.h>

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

// Defined in rkmk4.c
int rkmk4_integrate_cell(GridCell* cell, const IntegratorConfig* cfg, IntegratorWorkspace* ws);

// TODO: Define in future files
int rk4_integrate_cell(GridCell* cell, const IntegratorConfig* cfg, IntegratorWorkspace* ws);
int clebsch_integrate_cell(GridCell* cell, const IntegratorConfig* cfg, IntegratorWorkspace* ws);

/* ========================================================================
 * MAIN DISPATCHER
 * ======================================================================== */

int integrator_step_cell(GridCell* cell,
                         const IntegratorConfig* cfg,
                         integrator_e method,
                         IntegratorWorkspace* ws) {
    if (!cell || !cfg || !ws) return -1;

    // Dispatch to appropriate integrator
    switch (method) {
        case INTEGRATOR_RK4:
            // Classic RK4 (TODO: implement in rk4.c)
            return rk4_integrate_cell(cell, cfg, ws);

        case INTEGRATOR_RKMK4:
            // Geometric integrator for SE(3)
            return rkmk4_integrate_cell(cell, cfg, ws);

        case INTEGRATOR_CLEBSCH_COLLECTIVE:
            // Lie-Poisson symplectic (TODO: implement in clebsch_collective.c)
            return clebsch_integrate_cell(cell, cfg, ws);

        case INTEGRATOR_SYMPLECTIC_PRK:
            // Explicit symplectic (fallback for Clebsch)
            // TODO: implement
            return -4;

        case INTEGRATOR_EXPLICIT_EULER:
            // Simple 1st-order (testing only)
            // TODO: implement
            return -4;

        default:
            return -4;  // Unsupported method
    }
}

/* ========================================================================
 * ERROR ESTIMATION
 * ======================================================================== */

double estimate_integration_error(const GridCell* cell,
                                   const GridCell* prev_state,
                                   double dt) {
    if (!cell || !prev_state || dt <= 0.0) return INFINITY;

    // Simple error estimate: L2 norm of state difference
    double error_sq = 0.0;

    // Hydrology variables
    double d_theta = cell->theta - prev_state->theta;
    double d_sw = cell->surface_water - prev_state->surface_water;
    error_sq += d_theta * d_theta + d_sw * d_sw;

    // Soil variables
    double d_SOM = cell->SOM - prev_state->SOM;
    double d_T = cell->temperature - prev_state->temperature;
    error_sq += d_SOM * d_SOM + d_T * d_T;

    // Vegetation
    double d_veg = cell->vegetation - prev_state->vegetation;
    error_sq += d_veg * d_veg;

    // Momentum (if present)
    double d_mu = cell->momentum_u - prev_state->momentum_u;
    double d_mv = cell->momentum_v - prev_state->momentum_v;
    error_sq += d_mu * d_mu + d_mv * d_mv;

    // Scale by timestep (error should decrease with dt)
    return sqrt(error_sq) / dt;
}

/* ========================================================================
 * STUB IMPLEMENTATIONS (TEMPORARY)
 * ======================================================================== */

/**
 * Classic RK4 integrator (stub).
 *
 * TODO: Full implementation in rk4.c
 */
int rk4_integrate_cell(GridCell* cell, const IntegratorConfig* cfg, IntegratorWorkspace* ws) {
    (void)cell;
    (void)cfg;
    (void)ws;

    // Stub: no-op for now
    // Will be implemented when we add RK4 for LoD 0-1

    return 0;
}

/**
 * Clebsch-Collective integrator (stub).
 *
 * TODO: Full implementation in clebsch_collective.c (Week 2)
 */
int clebsch_integrate_cell(GridCell* cell, const IntegratorConfig* cfg, IntegratorWorkspace* ws) {
    (void)cell;
    (void)cfg;
    (void)ws;

    // Stub: no-op for now
    // Will be implemented in Phase 1.3 (Week 2)

    return 0;
}
