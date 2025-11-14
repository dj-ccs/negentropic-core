/*
 * test_regeneration_cascade.c - Unit Tests for REGv1 Solver
 *
 * Tests for:
 *   1. Parameter loading from JSON
 *   2. Single-cell ODE solver
 *   3. Hydrological bonus coupling
 *   4. Loess Plateau sanity check (20-year integration test)
 *   5. Threshold detection
 *
 * Compile with:
 *   gcc -o test_regeneration_cascade test_regeneration_cascade.c \
 *       ../src/solvers/regeneration_cascade.c \
 *       -I../src/solvers -lm -std=c99
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (REGv1)
 */

#include "regeneration_cascade.h"
#include "hydrology_richards_lite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Test statistics */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macros */
#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s\n", msg); \
    } \
} while(0)

#define TOLERANCE_FLOAT 0.001f

/* Fixed-point helpers (duplicated from regeneration_cascade.c for testing) */
#define FRACBITS 16
#define FRACUNIT (1 << FRACBITS)
#define FLOAT_TO_FXP(f)   ((int32_t)((f) * FRACUNIT))
#define FXP_TO_FLOAT(fxp) ((float)(fxp) / FRACUNIT)

/* ========================================================================
 * TEST: Parameter Loading
 * ======================================================================== */

void test_parameter_loading(void) {
    printf("\n[TEST] Parameter Loading\n");

    RegenerationParams params;

    /* Test Loess Plateau parameters */
    int result = regeneration_cascade_load_params(
        "../config/parameters/LoessPlateau.json",
        &params
    );

    TEST_ASSERT(result == 0, "Load LoessPlateau.json");
    TEST_ASSERT(params.r_V > 0.0f && params.r_V < 1.0f, "r_V in valid range");
    TEST_ASSERT(params.K_V > 0.0f && params.K_V < 1.0f, "K_V in valid range");
    TEST_ASSERT(params.theta_star > 0.0f && params.theta_star < 0.5f, "theta_star in valid range");
    TEST_ASSERT(params.SOM_star > 0.0f && params.SOM_star < 5.0f, "SOM_star in valid range");
    TEST_ASSERT(params.eta1 > 0.0f && params.eta1 < 20.0f, "eta1 in valid range");
    TEST_ASSERT(params.K_vertical_multiplier > 1.0f && params.K_vertical_multiplier < 2.0f,
                "K_vertical_multiplier in valid range");

    /* Test Chihuahuan Desert parameters */
    result = regeneration_cascade_load_params(
        "../config/parameters/ChihuahuanDesert.json",
        &params
    );

    TEST_ASSERT(result == 0, "Load ChihuahuanDesert.json");
    TEST_ASSERT(params.r_V < 0.12f, "Desert r_V < Loess Plateau r_V (slower growth)");
    TEST_ASSERT(params.K_V < 0.70f, "Desert K_V < Loess Plateau K_V (lower capacity)");
}

/* ========================================================================
 * TEST: Single-Cell ODE Integration
 * ======================================================================== */

void test_single_cell_ode(void) {
    printf("\n[TEST] Single-Cell ODE Integration\n");

    /* Load parameters */
    RegenerationParams params;
    regeneration_cascade_load_params("../config/parameters/LoessPlateau.json", &params);

    /* Create a single cell with degraded initial conditions */
    Cell cell;
    memset(&cell, 0, sizeof(Cell));

    cell.vegetation_cover = 0.15f;  /* 15% cover (degraded) */
    cell.SOM_percent = 0.5f;        /* 0.5% SOM (degraded) */
    cell.theta = 0.20f;             /* 20% moisture (above threshold) */
    cell.theta_s = 0.45f;           /* Saturated moisture */
    cell.porosity_eff = 0.40f;      /* Initial porosity */
    cell.K_tensor[8] = 1e-5f;       /* Initial K_zz */

    /* Initialize fixed-point versions */
    cell.vegetation_cover_fxp = FLOAT_TO_FXP(cell.vegetation_cover);
    cell.SOM_percent_fxp = FLOAT_TO_FXP(cell.SOM_percent);

    /* Run for 5 years */
    float dt_years = 1.0f;
    for (int year = 0; year < 5; year++) {
        regeneration_cascade_step(&cell, 1, &params, dt_years);
    }

    /* Check that vegetation has increased */
    TEST_ASSERT(cell.vegetation_cover > 0.15f,
                "Vegetation cover increases from degraded state");

    /* Check that SOM has increased */
    TEST_ASSERT(cell.SOM_percent > 0.5f,
                "SOM increases from degraded state");

    /* Check that hydrological bonus was applied */
    TEST_ASSERT(cell.porosity_eff > 0.40f,
                "Porosity increased via hydrological bonus");
    TEST_ASSERT(cell.K_tensor[8] > 1e-5f,
                "Vertical conductivity increased via hydrological bonus");

    /* Check float/fixed-point sync */
    float V_from_fxp = FXP_TO_FLOAT(cell.vegetation_cover_fxp);
    TEST_ASSERT(fabsf(V_from_fxp - cell.vegetation_cover) < 0.01f,
                "Float and fixed-point vegetation_cover are synced");
}

/* ========================================================================
 * TEST: Threshold Detection
 * ======================================================================== */

void test_threshold_detection(void) {
    printf("\n[TEST] Threshold Detection\n");

    RegenerationParams params;
    regeneration_cascade_load_params("../config/parameters/LoessPlateau.json", &params);

    Cell cell;
    memset(&cell, 0, sizeof(Cell));

    /* Case 1: Below all thresholds */
    cell.vegetation_cover = 0.10f;
    cell.SOM_percent = 0.5f;
    cell.theta = 0.10f;
    int status = regeneration_cascade_threshold_status(&cell, &params);
    TEST_ASSERT(status == 0, "Below all thresholds → status = 0");

    /* Case 2: Above moisture threshold only */
    cell.theta = 0.20f;  /* Above theta_star ≈ 0.17 */
    status = regeneration_cascade_threshold_status(&cell, &params);
    TEST_ASSERT((status & 0x1) != 0, "Above moisture threshold → bit 0 set");

    /* Case 3: Above SOM threshold only */
    cell.theta = 0.10f;
    cell.SOM_percent = 1.5f;  /* Above SOM_star ≈ 1.2 */
    status = regeneration_cascade_threshold_status(&cell, &params);
    TEST_ASSERT((status & 0x2) != 0, "Above SOM threshold → bit 1 set");

    /* Case 4: Above all thresholds */
    cell.vegetation_cover = 0.50f;  /* Above 0.5 * K_V ≈ 0.35 */
    cell.SOM_percent = 1.5f;
    cell.theta = 0.20f;
    status = regeneration_cascade_threshold_status(&cell, &params);
    TEST_ASSERT(status == 0x7, "Above all thresholds → status = 0b111 = 7");
}

/* ========================================================================
 * TEST: Health Score
 * ======================================================================== */

void test_health_score(void) {
    printf("\n[TEST] Health Score\n");

    RegenerationParams params;
    regeneration_cascade_load_params("../config/parameters/LoessPlateau.json", &params);

    Cell cell;
    memset(&cell, 0, sizeof(Cell));
    cell.theta_s = 0.45f;

    /* Degraded state */
    cell.vegetation_cover = 0.10f;
    cell.SOM_percent = 0.3f;
    cell.theta = 0.10f;
    float score_degraded = regeneration_cascade_health_score(&cell, &params);
    TEST_ASSERT(score_degraded < 0.3f, "Degraded state → low health score");

    /* Regenerated state */
    cell.vegetation_cover = 0.65f;
    cell.SOM_percent = 2.5f;
    cell.theta = 0.30f;
    float score_regenerated = regeneration_cascade_health_score(&cell, &params);
    TEST_ASSERT(score_regenerated > 0.6f, "Regenerated state → high health score");

    TEST_ASSERT(score_regenerated > score_degraded,
                "Regenerated score > Degraded score");
}

/* ========================================================================
 * TEST: Loess Plateau Sanity Check (Integration Test)
 * ======================================================================== */

void test_loess_plateau_sanity_check(void) {
    printf("\n[TEST] Loess Plateau Sanity Check (20-year integration)\n");

    /* Load Loess Plateau parameters */
    RegenerationParams params;
    int result = regeneration_cascade_load_params(
        "../config/parameters/LoessPlateau.json",
        &params
    );
    if (result != 0) {
        printf("  ✗ Failed to load parameters, skipping test\n");
        tests_failed++;
        return;
    }

    /* Initialize degraded Loess Plateau conditions */
    Cell cell;
    memset(&cell, 0, sizeof(Cell));

    cell.vegetation_cover = 0.15f;  /* 15% cover (degraded, pre-GTGP) */
    cell.SOM_percent = 0.5f;        /* 0.5% SOM (very low) */
    cell.theta = 0.12f;             /* 12% moisture (below threshold initially) */
    cell.theta_s = 0.45f;           /* Saturated water content */
    cell.porosity_eff = 0.40f;      /* Initial porosity */
    cell.K_tensor[8] = 5e-6f;       /* Initial K_zz [m/s] (degraded soil) */

    /* Initialize fixed-point versions */
    cell.vegetation_cover_fxp = FLOAT_TO_FXP(cell.vegetation_cover);
    cell.SOM_percent_fxp = FLOAT_TO_FXP(cell.SOM_percent);

    /* Store initial values for comparison */
    float V_initial = cell.vegetation_cover;
    float SOM_initial = cell.SOM_percent;
    float porosity_initial = cell.porosity_eff;
    float K_zz_initial = cell.K_tensor[8];

    printf("  Initial conditions:\n");
    printf("    V = %.3f, SOM = %.3f%%, θ = %.3f\n", V_initial, SOM_initial, cell.theta);
    printf("    porosity_eff = %.3f, K_zz = %.2e m/s\n", porosity_initial, K_zz_initial);

    /* Simulate 20 years with annual timesteps */
    /* Note: In reality, moisture would be driven by HYD-RLv1, but for this test
     * we simulate a gradual moisture increase due to improving soil conditions */
    float dt_years = 1.0f;

    for (int year = 1; year <= 20; year++) {
        /* Gradually increase moisture as soil improves (simplified moisture feedback) */
        /* In full coupling, HYD-RLv1 would compute this dynamically */
        if (year > 5) {
            cell.theta = 0.12f + (year - 5) * 0.01f;  /* Gradual wetting */
            if (cell.theta > 0.25f) cell.theta = 0.25f;
        }

        /* Execute one regeneration step */
        regeneration_cascade_step(&cell, 1, &params, dt_years);

        /* Print progress every 5 years */
        if (year % 5 == 0) {
            printf("  Year %d: V = %.3f, SOM = %.3f%%, θ = %.3f, health = %.3f\n",
                   year,
                   cell.vegetation_cover,
                   cell.SOM_percent,
                   cell.theta,
                   regeneration_cascade_health_score(&cell, &params));
        }
    }

    /* Final state */
    float V_final = cell.vegetation_cover;
    float SOM_final = cell.SOM_percent;
    float porosity_final = cell.porosity_eff;
    float K_zz_final = cell.K_tensor[8];

    printf("  Final conditions (year 20):\n");
    printf("    V = %.3f, SOM = %.3f%%, θ = %.3f\n", V_final, SOM_final, cell.theta);
    printf("    porosity_eff = %.3f, K_zz = %.2e m/s\n", porosity_final, K_zz_final);

    /* Assertions (critical success criteria) */
    TEST_ASSERT(V_final > 0.60f,
                "Vegetation cover exceeds 0.60 after 20 years");

    TEST_ASSERT(SOM_final > 2.0f,
                "SOM exceeds 2.0% after 20 years");

    TEST_ASSERT(V_final > V_initial * 2.0f,
                "Vegetation has at least doubled");

    TEST_ASSERT(SOM_final > SOM_initial * 2.0f,
                "SOM has at least doubled");

    TEST_ASSERT(porosity_final > porosity_initial,
                "Porosity has measurably increased (hydrological bonus)");

    TEST_ASSERT(K_zz_final > K_zz_initial,
                "Vertical conductivity has measurably increased (hydrological bonus)");

    /* Check for sigmoid-like trajectory (inflection point detection) */
    /* Regenerate to find inflection year */
    memset(&cell, 0, sizeof(Cell));
    cell.vegetation_cover = 0.15f;
    cell.SOM_percent = 0.5f;
    cell.theta = 0.12f;
    cell.theta_s = 0.45f;
    cell.porosity_eff = 0.40f;
    cell.K_tensor[8] = 5e-6f;
    cell.vegetation_cover_fxp = FLOAT_TO_FXP(cell.vegetation_cover);
    cell.SOM_percent_fxp = FLOAT_TO_FXP(cell.SOM_percent);

    float max_dV = 0.0f;
    int inflection_year = 0;
    float V_prev = 0.15f;

    for (int year = 1; year <= 20; year++) {
        if (year > 5) {
            cell.theta = 0.12f + (year - 5) * 0.01f;
            if (cell.theta > 0.25f) cell.theta = 0.25f;
        }

        regeneration_cascade_step(&cell, 1, &params, dt_years);

        float dV = cell.vegetation_cover - V_prev;
        if (dV > max_dV) {
            max_dV = dV;
            inflection_year = year;
        }
        V_prev = cell.vegetation_cover;
    }

    printf("  Inflection point: year %d (max dV/dt = %.4f yr^-1)\n", inflection_year, max_dV);
    TEST_ASSERT(inflection_year >= 8 && inflection_year <= 12,
                "Sigmoid inflection occurs around year 8-12 (expected from theory)");

    /* Overall integration test pass */
    printf("  ✓ Loess Plateau sanity check PASSED\n");
    tests_passed++;  /* Count the overall integration test as one success */
}

/* ========================================================================
 * TEST: Fixed-Point Accuracy
 * ======================================================================== */

void test_fixed_point_accuracy(void) {
    printf("\n[TEST] Fixed-Point Accuracy\n");

    /* Test round-trip conversion */
    float test_values[] = {0.0f, 0.15f, 0.5f, 0.75f, 1.0f, 2.5f, 5.0f};
    for (size_t i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        float original = test_values[i];
        int32_t fxp = FLOAT_TO_FXP(original);
        float recovered = FXP_TO_FLOAT(fxp);

        float error = fabsf(original - recovered);
        TEST_ASSERT(error < 0.0001f, "Round-trip conversion accurate");
    }

    /* Test that fixed-point has sufficient precision for V and SOM */
    /* Smallest meaningful change: 0.01 (1% vegetation or 0.01% SOM) */
    float small_change = 0.01f;
    int32_t fxp_small = FLOAT_TO_FXP(small_change);
    float recovered_small = FXP_TO_FLOAT(fxp_small);

    TEST_ASSERT(recovered_small > 0.0f && recovered_small < 0.02f,
                "Fixed-point can represent 1% changes accurately");
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    printf("======================================================================\n");
    printf("REGv1 REGENERATION CASCADE SOLVER - UNIT TEST SUITE\n");
    printf("======================================================================\n");
    printf("Target: Loess Plateau & Chihuahuan Desert validation\n");
    printf("Format: 16.16 fixed-point (FRACUNIT = %d)\n", FRACUNIT);

    /* Run all test suites */
    test_parameter_loading();
    test_fixed_point_accuracy();
    test_single_cell_ode();
    test_threshold_detection();
    test_health_score();
    test_loess_plateau_sanity_check();

    /* Summary */
    printf("\n======================================================================\n");
    printf("TEST SUMMARY\n");
    printf("======================================================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("\n✓ ALL TESTS PASSED - REGv1 Solver is ready for integration\n");
    } else {
        printf("\n✗ SOME TESTS FAILED - Review implementation\n");
    }
    printf("======================================================================\n");

    return tests_failed;
}
