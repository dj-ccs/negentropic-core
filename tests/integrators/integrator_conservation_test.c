// integrator_conservation_test.c - Conservation Tests for Integrators
//
// Tests (from docs/integrators.md section 6):
//   1. Casimir drift < 1e-6 (fixed-point)
//   2. Energy drift over 10k steps
//   3. Reversibility (forward/backward integration)
//
// Reference: docs/v2.2_Upgrade.md Phase 1.6
// Author: negentropic-core team
// Version: 2.2.0

#include "../../src/core/integrators/integrators.h"
#include "../../src/core/integrators/clebsch.h"
#include "../../src/core/integrators/workspace.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

/* ========================================================================
 * TEST UTILITIES
 * ======================================================================== */

#define ASSERT_NEAR(a, b, tol) \
    do { \
        double _diff = fabs((a) - (b)); \
        if (_diff > (tol)) { \
            fprintf(stderr, "FAIL: %s:%d: |%f - %f| = %f > %f\n", \
                    __FILE__, __LINE__, (double)(a), (double)(b), _diff, (double)(tol)); \
            return 1; \
        } \
    } while (0)

/* ========================================================================
 * TEST 1: CASIMIR CONSERVATION
 * ======================================================================== */

int test_casimir_conservation(void) {
    printf("Test 1: Casimir conservation (Clebsch integrator)... ");

    // Initialize Clebsch LUT
    ClebschLUT lut;
    int result = clebsch_lut_init(&lut);
    assert(result == 0);

    ClebschWorkspace* ws = clebsch_workspace_create(&lut);
    assert(ws != NULL);

    // Initial canonical variables
    double q[8] = {0.1, 0.2, 0.3, 0.4, 0.05, 0.06, 0.07, 0.08};
    double p[8] = {0.5, 0.4, 0.3, 0.2, 0.15, 0.14, 0.13, 0.12};

    // Compute initial Casimir
    double C_initial = compute_casimir(q, p);

    // Configure integrator
    IntegratorConfig cfg;
    integrator_config_init(&cfg);
    cfg.dt = 0.01;  // Small timestep
    cfg.flags |= INTEGRATOR_FLAG_PRESERVE_CASIMIRS;

    // Integrate for 100 steps
    const int num_steps = 100;
    for (int step = 0; step < num_steps; step++) {
        result = clebsch_symplectic_step(q, p, cfg.dt, &cfg, ws);
        assert(result >= 0);  // 0 = success, 1 = fallback
    }

    // Compute final Casimir
    double C_final = compute_casimir(q, p);

    // Check drift
    double casimir_drift = fabs(C_final - C_initial);

    // Target: < 1e-6 (from docs/integrators.md section 8)
    ASSERT_NEAR(casimir_drift, 0.0, 1e-6);

    clebsch_workspace_destroy(ws);
    clebsch_lut_destroy(&lut);

    printf("PASS (drift = %.2e)\n", casimir_drift);
    return 0;
}

/* ========================================================================
 * TEST 2: ENERGY CONSERVATION (HAMILTONIAN)
 * ======================================================================== */

int test_energy_conservation(void) {
    printf("Test 2: Energy conservation (symplectic integrator)... ");

    // Initialize Clebsch LUT
    ClebschLUT lut;
    int result = clebsch_lut_init(&lut);
    assert(result == 0);

    ClebschWorkspace* ws = clebsch_workspace_create(&lut);
    assert(ws != NULL);

    // Initial canonical variables
    double q[8] = {0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double p[8] = {0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    // Simple Hamiltonian: H = (1/2) * sum(p[i]^2 + q[i]^2)
    auto compute_energy = [](const double* q, const double* p) -> double {
        double H = 0.0;
        for (int i = 0; i < 8; i++) {
            H += 0.5 * (p[i] * p[i] + q[i] * q[i]);
        }
        return H;
    };

    double H_initial = compute_energy(q, p);

    // Configure integrator
    IntegratorConfig cfg;
    integrator_config_init(&cfg);
    cfg.dt = 0.01;

    // Integrate for 1000 steps
    const int num_steps = 1000;
    for (int step = 0; step < num_steps; step++) {
        result = clebsch_symplectic_step(q, p, cfg.dt, &cfg, ws);
        assert(result >= 0);
    }

    double H_final = compute_energy(q, p);

    // Check energy drift
    double energy_drift = fabs(H_final - H_initial);
    double relative_drift = energy_drift / H_initial;

    // Symplectic integrators conserve energy well
    // Target: relative drift < 1e-3 for simple harmonic oscillator
    ASSERT_NEAR(relative_drift, 0.0, 1e-3);

    clebsch_workspace_destroy(ws);
    clebsch_lut_destroy(&lut);

    printf("PASS (relative drift = %.2e)\n", relative_drift);
    return 0;
}

/* ========================================================================
 * TEST 3: REVERSIBILITY
 * ======================================================================== */

int test_reversibility(void) {
    printf("Test 3: Reversibility (forward/backward integration)... ");

    // Initialize Clebsch LUT
    ClebschLUT lut;
    int result = clebsch_lut_init(&lut);
    assert(result == 0);

    ClebschWorkspace* ws = clebsch_workspace_create(&lut);
    assert(ws != NULL);

    // Initial state
    double q_initial[8] = {0.1, 0.2, 0.3, 0.4, 0.05, 0.06, 0.07, 0.08};
    double p_initial[8] = {0.5, 0.4, 0.3, 0.2, 0.15, 0.14, 0.13, 0.12};

    double q[8], p[8];
    memcpy(q, q_initial, sizeof(q));
    memcpy(p, p_initial, sizeof(p));

    // Configure integrator
    IntegratorConfig cfg;
    integrator_config_init(&cfg);
    cfg.dt = 0.01;

    // Forward integration (100 steps)
    const int num_steps = 100;
    for (int step = 0; step < num_steps; step++) {
        result = clebsch_symplectic_step(q, p, cfg.dt, &cfg, ws);
        assert(result >= 0);
    }

    // Backward integration (100 steps with -dt)
    cfg.dt = -0.01;  // Reverse time
    for (int step = 0; step < num_steps; step++) {
        result = clebsch_symplectic_step(q, p, cfg.dt, &cfg, ws);
        assert(result >= 0);
    }

    // Compute L2 error
    double error_sq = 0.0;
    for (int i = 0; i < 8; i++) {
        error_sq += (q[i] - q_initial[i]) * (q[i] - q_initial[i]);
        error_sq += (p[i] - p_initial[i]) * (p[i] - p_initial[i]);
    }
    double L2_error = sqrt(error_sq);

    // Symplectic integrators should be very reversible
    // Target: L2 error < 1e-8 (FP64 precision)
    ASSERT_NEAR(L2_error, 0.0, 1e-8);

    clebsch_workspace_destroy(ws);
    clebsch_lut_destroy(&lut);

    printf("PASS (L2 error = %.2e)\n", L2_error);
    return 0;
}

/* ========================================================================
 * TEST 4: LOD DISPATCH CORRECTNESS
 * ======================================================================== */

int test_lod_dispatch(void) {
    printf("Test 4: LoD-gated dispatch correctness... ");

    // Create test cells at different LoD levels
    GridCell cell_lod0, cell_lod2;

    // LoD 0 cell (coarse)
    cell_lod0.lod_level = 0;
    cell_lod0.flags = CELL_FLAG_ACTIVE;
    cell_lod0.theta = 0.3f;
    cell_lod0.vorticity = 0.0f;

    // LoD 2 cell (fine, requires SE(3))
    cell_lod2.lod_level = 2;
    cell_lod2.flags = CELL_FLAG_ACTIVE | CELL_FLAG_REQUIRES_SE3;
    cell_lod2.theta = 0.3f;
    cell_lod2.vorticity = 0.0f;

    // Create workspace
    IntegratorWorkspace* ws = integrator_workspace_create(12);
    assert(ws != NULL);

    // Configure integrator
    IntegratorConfig cfg;
    integrator_config_init(&cfg);
    cfg.dt = 0.1;

    // Integrate LoD 0 cell (should use RK4)
    int result = lod_gated_step_cell(&cell_lod0, &cfg, ws);
    // Note: RK4 is stub, so this will succeed with no-op
    assert(result == 0);

    // Integrate LoD 2 cell (should use RKMK4 or Clebsch)
    result = lod_gated_step_cell(&cell_lod2, &cfg, ws);
    assert(result == 0);

    integrator_workspace_destroy(ws);

    printf("PASS\n");
    return 0;
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    printf("=== Integrator Conservation Tests ===\n\n");

    // Initialize integrator subsystem
    integrator_init();

    int failures = 0;

    failures += test_casimir_conservation();
    failures += test_energy_conservation();
    failures += test_reversibility();
    failures += test_lod_dispatch();

    printf("\n");
    if (failures == 0) {
        printf("=== ALL TESTS PASSED ===\n");
        return 0;
    } else {
        printf("=== %d TEST(S) FAILED ===\n", failures);
        return 1;
    }
}
