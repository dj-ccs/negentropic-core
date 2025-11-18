// workspace.c - Integrator Workspace Management
//
// Memory management for integrator scratch space.
// One allocation per worker, reused across steps.
//
// Author: negentropic-core team
// Version: 2.2.0

#include "integrators.h"
#include "workspace.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * WORKSPACE MANAGEMENT
 * ======================================================================== */

IntegratorWorkspace* integrator_workspace_create(size_t max_dim) {
    if (max_dim == 0 || max_dim > 128) {
        return NULL;  // Sanity check: dimension must be reasonable
    }

    IntegratorWorkspace* ws = (IntegratorWorkspace*)calloc(1, sizeof(IntegratorWorkspace));
    if (!ws) {
        return NULL;
    }

    ws->max_dim = max_dim;
    ws->lut_initialized = false;
    ws->step_count = 0;
    ws->fallback_count = 0;
    ws->max_error = 0.0;

    // LUT handles will be initialized on first use
    ws->clebsch_lut = NULL;
    ws->exp_lut = NULL;

    return ws;
}

void integrator_workspace_destroy(IntegratorWorkspace* ws) {
    if (!ws) return;

    // TODO: Free LUT handles if allocated
    // For now, they're NULL so no action needed

    free(ws);
}

void integrator_workspace_reset(IntegratorWorkspace* ws) {
    if (!ws) return;

    // Zero out all scratch buffers
    memset(ws->k1, 0, sizeof(ws->k1));
    memset(ws->k2, 0, sizeof(ws->k2));
    memset(ws->k3, 0, sizeof(ws->k3));
    memset(ws->k4, 0, sizeof(ws->k4));
    memset(ws->exp_scratch, 0, sizeof(ws->exp_scratch));
    memset(ws->twist_temp, 0, sizeof(ws->twist_temp));

    memset(ws->q_temp, 0, sizeof(ws->q_temp));
    memset(ws->p_temp, 0, sizeof(ws->p_temp));
    memset(ws->q_stage, 0, sizeof(ws->q_stage));
    memset(ws->p_stage, 0, sizeof(ws->p_stage));
    memset(ws->force_buffer, 0, sizeof(ws->force_buffer));

    memset(ws->rk4_k1, 0, sizeof(ws->rk4_k1));
    memset(ws->rk4_k2, 0, sizeof(ws->rk4_k2));
    memset(ws->rk4_k3, 0, sizeof(ws->rk4_k3));
    memset(ws->rk4_k4, 0, sizeof(ws->rk4_k4));
    memset(ws->rk4_temp, 0, sizeof(ws->rk4_temp));

    ws->casimir_initial = 0.0;

    // Don't reset statistics or LUT handles
}

/* ========================================================================
 * CONFIGURATION HELPERS
 * ======================================================================== */

void integrator_config_init(IntegratorConfig* cfg) {
    if (!cfg) return;

    cfg->dt = 0.1;           // Default: 0.1 seconds
    cfg->max_iter = 4;       // Default: 4 Newton iterations
    cfg->tol = 1e-6;         // Default: 1e-6 tolerance
    cfg->flags = INTEGRATOR_FLAG_PRESERVE_CASIMIRS | INTEGRATOR_FLAG_USE_LUT_ACCEL;
}

void integrator_config_set_dt(IntegratorConfig* cfg, double dt) {
    if (!cfg || dt <= 0.0) return;
    cfg->dt = dt;
}

void integrator_config_set_preserve_casimirs(IntegratorConfig* cfg, bool enable) {
    if (!cfg) return;

    if (enable) {
        cfg->flags |= INTEGRATOR_FLAG_PRESERVE_CASIMIRS;
    } else {
        cfg->flags &= ~INTEGRATOR_FLAG_PRESERVE_CASIMIRS;
    }
}

/* ========================================================================
 * INITIALIZATION
 * ======================================================================== */

void integrator_init(void) {
    // TODO: Initialize global LUT tables
    // - Load exp_map LUT (8192 entries, 32 KB)
    // - Load Clebsch LUT (256-512 entries, 4-8 KB)
    //
    // For now, this is a no-op. LUTs will be generated in Phase 1.3.
}
