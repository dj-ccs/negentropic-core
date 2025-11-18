// workspace.h - Integrator Workspace Internal Structure
//
// Scratch space for integrators to avoid dynamic allocation in hot loops.
// One workspace per worker thread, reused across integration steps.
//
// Author: negentropic-core team
// Version: 2.2.0

#ifndef NEG_INTEGRATOR_WORKSPACE_H
#define NEG_INTEGRATOR_WORKSPACE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * WORKSPACE STRUCTURE
 * ======================================================================== */

/**
 * Integrator workspace (internal representation).
 *
 * Contains scratch buffers for different integrator types.
 * Size: ~1-2 KB total
 */
struct IntegratorWorkspace {
    // RKMK4 scratch space (SE(3) integration)
    double k1[6];              // Lie algebra stage 1 (twist)
    double k2[6];              // Lie algebra stage 2
    double k3[6];              // Lie algebra stage 3
    double k4[6];              // Lie algebra stage 4
    double exp_scratch[9];     // Temporary for exp_map (3x3 matrix)
    double twist_temp[6];      // Temporary twist vector

    // Clebsch-Collective scratch space (Lie-Poisson)
    double q_temp[8];          // Canonical position variables
    double p_temp[8];          // Canonical momentum variables
    double q_stage[8];         // Stage values for q
    double p_stage[8];         // Stage values for p
    double force_buffer[8];    // Force evaluation buffer
    double casimir_initial;    // Initial Casimir value (for enforcement)

    // RK4 scratch space (classic integration)
    double rk4_k1[12];         // Stage 1 (max 12 state variables)
    double rk4_k2[12];         // Stage 2
    double rk4_k3[12];         // Stage 3
    double rk4_k4[12];         // Stage 4
    double rk4_temp[12];       // Temporary state

    // LUT handles (opaque pointers to precomputed tables)
    void* clebsch_lut;         // Clebsch lift/project LUT
    void* exp_lut;             // Exponential map LUT

    // Configuration
    size_t max_dim;            // Maximum dimension allocated
    bool lut_initialized;      // LUT tables loaded

    // Statistics (optional, for debugging)
    uint64_t step_count;       // Number of integration steps
    uint64_t fallback_count;   // Number of fallbacks to explicit method
    double max_error;          // Maximum error encountered
};

typedef struct IntegratorWorkspace IntegratorWorkspace;

#ifdef __cplusplus
}
#endif

#endif /* NEG_INTEGRATOR_WORKSPACE_H */
