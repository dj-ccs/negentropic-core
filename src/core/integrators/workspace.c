// workspace.c - Integrator Workspace Management (Doom Ethos v2.2)
//
// Memory management for integrator scratch space.
// Uses slab allocator for zero-malloc determinism.
//
// Author: negentropic-core team
// Version: 2.2.0 (Doom Ethos upgrade)

#include "integrators.h"
#include "workspace.h"
#include "workspace_slab.h"
#include <string.h>

/* ========================================================================
 * WORKSPACE MANAGEMENT (DOOM ETHOS: NO MALLOC IN HOT PATHS)
 * ======================================================================== */

IntegratorWorkspace* integrator_workspace_create(size_t max_dim) {
    if (max_dim == 0 || max_dim > 128) {
        return NULL;  // Sanity check: dimension must be reasonable
    }

    // DOOM ETHOS: Use slab allocator instead of calloc
    IntegratorWorkspace* ws = workspace_slab_alloc_integrator();
    if (!ws) {
        return NULL;  // Pool exhausted
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

    // Validate this is from our slab (catch double-free bugs)
    if (!workspace_slab_validate_integrator(ws)) {
        return;  // Not from slab, don't free
    }

    // DOOM ETHOS: Return to slab pool instead of free
    workspace_slab_free_integrator(ws);
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
 * INITIALIZATION (DOOM ETHOS)
 * ======================================================================== */

void integrator_init(void) {
    // DOOM ETHOS: Initialize slab allocator for zero-malloc workspaces
    workspace_slab_init();

    // TODO: Initialize global LUT tables
    // - Load exp_map LUT (8192 entries, 32 KB)
    // - Load Clebsch LUT (256-512 entries, 4-8 KB)
    //
    // For now, Clebsch LUT is initialized via slab allocator.
}
