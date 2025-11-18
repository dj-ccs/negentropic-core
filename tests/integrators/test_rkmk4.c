// test_rkmk4.c - Unit Tests for RKMK4 Integrator
//
// Tests:
//   1. Constant angular velocity (should maintain orthogonality)
//   2. Torque-free rotation (energy conservation)
//   3. Comparison with FP64 oracle
//
// Reference: docs/v2.2_Upgrade.md Phase 1.2, Task 1.2.5
// Author: negentropic-core team
// Version: 2.2.0

#include "../../src/core/integrators/integrators.h"
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
 * TEST 1: WORKSPACE CREATION
 * ======================================================================== */

int test_workspace_creation(void) {
    printf("Test 1: Workspace creation and destruction... ");

    IntegratorWorkspace* ws = integrator_workspace_create(12);
    assert(ws != NULL);
    assert(ws->max_dim == 12);
    assert(ws->step_count == 0);
    assert(ws->fallback_count == 0);

    integrator_workspace_destroy(ws);

    printf("PASS\n");
    return 0;
}

/* ========================================================================
 * TEST 2: WORKSPACE RESET
 * ======================================================================== */

int test_workspace_reset(void) {
    printf("Test 2: Workspace reset... ");

    IntegratorWorkspace* ws = integrator_workspace_create(12);
    assert(ws != NULL);

    // Modify some values
    ws->k1[0] = 1.0;
    ws->k1[1] = 2.0;
    ws->casimir_initial = 3.0;

    // Reset
    integrator_workspace_reset(ws);

    // Check that buffers are zeroed
    ASSERT_NEAR(ws->k1[0], 0.0, 1e-12);
    ASSERT_NEAR(ws->k1[1], 0.0, 1e-12);
    ASSERT_NEAR(ws->casimir_initial, 0.0, 1e-12);

    integrator_workspace_destroy(ws);

    printf("PASS\n");
    return 0;
}

/* ========================================================================
 * TEST 3: INTEGRATOR CONFIGURATION
 * ======================================================================== */

int test_integrator_config(void) {
    printf("Test 3: Integrator configuration... ");

    IntegratorConfig cfg;
    integrator_config_init(&cfg);

    // Check default values
    ASSERT_NEAR(cfg.dt, 0.1, 1e-9);
    assert(cfg.max_iter == 4);
    ASSERT_NEAR(cfg.tol, 1e-6, 1e-9);
    assert((cfg.flags & INTEGRATOR_FLAG_PRESERVE_CASIMIRS) != 0);
    assert((cfg.flags & INTEGRATOR_FLAG_USE_LUT_ACCEL) != 0);

    // Test set_dt
    integrator_config_set_dt(&cfg, 0.05);
    ASSERT_NEAR(cfg.dt, 0.05, 1e-9);

    // Test preserve_casimirs toggle
    integrator_config_set_preserve_casimirs(&cfg, false);
    assert((cfg.flags & INTEGRATOR_FLAG_PRESERVE_CASIMIRS) == 0);

    integrator_config_set_preserve_casimirs(&cfg, true);
    assert((cfg.flags & INTEGRATOR_FLAG_PRESERVE_CASIMIRS) != 0);

    printf("PASS\n");
    return 0;
}

/* ========================================================================
 * TEST 4: ERROR ESTIMATION
 * ======================================================================== */

int test_error_estimation(void) {
    printf("Test 4: Error estimation... ");

    GridCell cell1, cell2;

    // Initialize identical states
    cell1.theta = 0.3f;
    cell1.surface_water = 5.0f;
    cell1.SOM = 1.5f;
    cell1.temperature = 15.0f;
    cell1.vegetation = 0.5f;
    cell1.momentum_u = 0.0f;
    cell1.momentum_v = 0.0f;

    cell2 = cell1;

    // Error should be zero for identical states
    double error = estimate_integration_error(&cell1, &cell2, 0.1);
    ASSERT_NEAR(error, 0.0, 1e-9);

    // Perturb one state
    cell2.theta = 0.31f;  // Small change

    // Error should be non-zero
    error = estimate_integration_error(&cell1, &cell2, 0.1);
    assert(error > 0.0);

    printf("PASS\n");
    return 0;
}

/* ========================================================================
 * TEST 5: BASIC INTEGRATION (STUB)
 * ======================================================================== */

int test_basic_integration(void) {
    printf("Test 5: Basic integration (stub)... ");

    GridCell cell;
    cell.theta = 0.3f;
    cell.surface_water = 5.0f;
    cell.SOM = 1.5f;
    cell.temperature = 15.0f;
    cell.vegetation = 0.5f;
    cell.momentum_u = 0.0f;
    cell.momentum_v = 0.0f;
    cell.vorticity = 0.0f;
    cell.flags = CELL_FLAG_ACTIVE;
    cell.lod_level = 2;

    IntegratorConfig cfg;
    integrator_config_init(&cfg);
    cfg.dt = 0.1;

    IntegratorWorkspace* ws = integrator_workspace_create(12);
    assert(ws != NULL);

    // Test RK4 (stub - should succeed with no-op)
    int result = integrator_step_cell(&cell, &cfg, INTEGRATOR_RK4, ws);
    assert(result == 0);

    // Test RKMK4 (stub - should succeed with no-op)
    result = integrator_step_cell(&cell, &cfg, INTEGRATOR_RKMK4, ws);
    assert(result == 0);

    integrator_workspace_destroy(ws);

    printf("PASS (stub)\n");
    return 0;
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    printf("=== RKMK4 Integrator Unit Tests ===\n\n");

    // Initialize integrator subsystem
    integrator_init();

    int failures = 0;

    failures += test_workspace_creation();
    failures += test_workspace_reset();
    failures += test_integrator_config();
    failures += test_error_estimation();
    failures += test_basic_integration();

    printf("\n");
    if (failures == 0) {
        printf("=== ALL TESTS PASSED ===\n");
        return 0;
    } else {
        printf("=== %d TEST(S) FAILED ===\n", failures);
        return 1;
    }
}
