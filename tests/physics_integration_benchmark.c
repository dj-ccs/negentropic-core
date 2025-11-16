/*
 * physics_integration_benchmark.c - Physics Integration & Performance Benchmark
 *
 * PURPOSE:
 *   This program wires up the real HYD-RLv1 and REGv1 physics solvers,
 *   runs them on a 32x32 grid for 100 timesteps, and measures performance
 *   to validate that WASM can run fast enough for real-time browser use.
 *
 * CRITICAL DELIVERABLE:
 *   This benchmark determines the entire project's next phase based on:
 *   - < 150 ns/cell/step: Excellent, proceed to visualization
 *   - 150-250 ns/cell/step: Acceptable, proceed with Doom Engine optimization
 *   - > 250 ns/cell/step: Problem, halt visualization, optimize immediately
 *
 * IMPLEMENTATION:
 *   1. Create 32x32 grid of Cell structs (1024 cells total)
 *   2. Load initial conditions from LoessPlateau.json parameters
 *   3. Run simulation loop for 100 timesteps:
 *      - Call richards_lite_step() every step
 *      - Call regeneration_cascade_step() every 128 steps
 *   4. Measure wall-clock time with nanosecond precision
 *   5. Calculate and report: ns/cell/step
 *   6. Verify physics: print sample cell states before/after
 *
 * Author: negentropic-core team (ClaudeCode)
 * Version: 0.1.0 (PHYS-INT Sprint)
 * License: MIT OR GPL-3.0
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Solver headers */
#include "../src/solvers/hydrology_richards_lite.h"
#include "../src/solvers/regeneration_cascade.h"

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

#define GRID_SIZE 32
#define NUM_CELLS (GRID_SIZE * GRID_SIZE)
#define NUM_STEPS 100
#define REG_CALL_INTERVAL 128  /* Call REG every 128 HYD steps */

/* Hardcoded parameters from LoessPlateau.json */
#define PARAM_R_V 0.12f              /* Vegetation growth rate [yr^-1] */
#define PARAM_K_V 0.70f              /* Vegetation carrying capacity */
#define PARAM_LAMBDA1 0.50f          /* Moisture-to-vegetation coupling */
#define PARAM_LAMBDA2 0.08f          /* SOM-to-vegetation coupling */
#define PARAM_THETA_STAR 0.17f       /* Critical soil moisture threshold */
#define PARAM_SOM_STAR 1.2f          /* Critical SOM threshold */
#define PARAM_A1 0.18f               /* SOM input rate */
#define PARAM_A2 0.035f              /* SOM decay rate */
#define PARAM_ETA1 5.0f              /* Water holding capacity gain [mm per %SOM] */
#define PARAM_K_VERT_MULT 1.15f      /* K_zz multiplier per %SOM */

/* Initial conditions for degraded state (validation targets) */
#define INITIAL_VEG_COVER 0.15f
#define INITIAL_SOM_PERCENT 0.5f
#define INITIAL_THETA 0.12f

/* Soil hydraulic properties (typical loess plateau values) */
#define K_S 5.0e-6f            /* Saturated hydraulic conductivity [m/s] */
#define ALPHA_VG 1.5f          /* van Genuchten alpha [1/m] */
#define N_VG 1.4f              /* van Genuchten n */
#define THETA_S 0.45f          /* Saturated water content */
#define THETA_R 0.05f          /* Residual water content */

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * Get high-precision timestamp in nanoseconds.
 */
static uint64_t get_time_ns(void) {
#ifdef __EMSCRIPTEN__
    /* For WASM, use emscripten_get_now() if needed */
    /* For now, use a simple fallback */
    return (uint64_t)(clock() * 1000000000ULL / CLOCKS_PER_SEC);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/**
 * Initialize a single Cell with degraded initial conditions.
 */
static void init_cell(Cell* cell, int x, int y) {
    /* Hydrological state (degraded/dry) */
    cell->theta = INITIAL_THETA;
    cell->psi = -10.0f;  /* Dry soil (negative pressure) */
    cell->h_surface = 0.0f;
    cell->zeta = 0.0f;

    /* Soil hydraulic parameters (loess plateau) */
    cell->K_s = K_S;
    cell->alpha_vG = ALPHA_VG;
    cell->n_vG = N_VG;
    cell->theta_s = THETA_S;
    cell->theta_r = THETA_R;

    /* Intervention multipliers (no intervention initially) */
    cell->M_K_zz = 1.0f;
    cell->M_K_xx = 1.0f;
    cell->kappa_evap = 1.0f;
    cell->Delta_zeta = 0.0f;

    /* Microtopography parameters */
    cell->zeta_c = 0.005f;  /* 5mm fill-and-spill threshold */
    cell->a_c = 0.5f;

    /* Regeneration state (degraded) */
    cell->vegetation_cover = INITIAL_VEG_COVER;
    cell->SOM_percent = INITIAL_SOM_PERCENT;

    /* Fixed-point versions */
    cell->vegetation_cover_fxp = (int32_t)(INITIAL_VEG_COVER * 65536.0f);
    cell->SOM_percent_fxp = (int32_t)(INITIAL_SOM_PERCENT * 65536.0f);

    /* Effective parameters (will be modified by REGv1) */
    cell->porosity_eff = THETA_S;  /* Start at base porosity */

    /* Initialize K_tensor (isotropic, diagonal only) */
    memset(cell->K_tensor, 0, sizeof(cell->K_tensor));
    cell->K_tensor[0] = K_S;  /* Kxx */
    cell->K_tensor[4] = K_S;  /* Kyy */
    cell->K_tensor[8] = K_S;  /* Kzz */

    /* Grid geometry */
    cell->z = 0.0f;  /* Flat terrain */
    cell->dz = 0.2f;  /* 20cm soil layer */
    cell->dx = 10.0f;  /* 10m grid spacing */

    /* REGv2 fields (zero for now, not used in v1) */
    cell->C_labile = 0.0f;
    cell->soil_temp_C = 15.0f;  /* Moderate temperature */
    cell->N_fix = 0.0f;
    cell->Phi_agg = 0.5f;
    cell->FB_ratio = 0.5f;
    cell->Phi_hyphae = 0.0f;
    cell->O2 = 1.0f;
    cell->C_sup = 0.0f;
    cell->LAI = 0.0f;
    cell->n_cond_neighbors = 0;
    cell->theta_deep = THETA_R;

    /* Suppress unused parameter warnings */
    (void)x;
    (void)y;
}

/**
 * Print summary of a cell's state (for verification).
 */
static void print_cell_state(const Cell* cell, int x, int y) {
    printf("  Cell[%d,%d]: theta=%.4f, psi=%.2f, V=%.3f, SOM=%.2f%%, porosity_eff=%.4f, Kzz=%.2e\n",
           x, y, cell->theta, cell->psi,
           cell->vegetation_cover, cell->SOM_percent,
           cell->porosity_eff, cell->K_tensor[8]);
}

/* ========================================================================
 * MAIN BENCHMARK
 * ======================================================================== */

int main(void) {
    printf("========================================================================\n");
    printf("PHYSICS INTEGRATION & PERFORMANCE BENCHMARK\n");
    printf("========================================================================\n");
    printf("\n");
    printf("Configuration:\n");
    printf("  Grid size:        %dx%d (%d cells)\n", GRID_SIZE, GRID_SIZE, NUM_CELLS);
    printf("  Timesteps:        %d\n", NUM_STEPS);
    printf("  REG call freq:    every %d steps\n", REG_CALL_INTERVAL);
    printf("\n");

    /* ====================================================================
     * STEP 1: Initialize Grid
     * ==================================================================== */

    printf("Step 1: Initializing grid...\n");

    Cell* grid = (Cell*)calloc(NUM_CELLS, sizeof(Cell));
    if (!grid) {
        fprintf(stderr, "ERROR: Failed to allocate grid memory\n");
        return 1;
    }

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            init_cell(&grid[y * GRID_SIZE + x], x, y);
        }
    }

    printf("  ✓ Grid initialized\n");

    /* ====================================================================
     * STEP 2: Initialize Solvers
     * ==================================================================== */

    printf("\nStep 2: Initializing solvers...\n");

    /* Initialize Richards-Lite lookup tables */
    richards_lite_init();
    printf("  ✓ HYD-RLv1 initialized (lookup tables generated)\n");

    /* Load REGv1 parameters */
    RegenerationParams reg_params;
    reg_params.r_V = PARAM_R_V;
    reg_params.K_V = PARAM_K_V;
    reg_params.lambda1 = PARAM_LAMBDA1;
    reg_params.lambda2 = PARAM_LAMBDA2;
    reg_params.theta_star = PARAM_THETA_STAR;
    reg_params.SOM_star = PARAM_SOM_STAR;
    reg_params.a1 = PARAM_A1;
    reg_params.a2 = PARAM_A2;
    reg_params.eta1 = PARAM_ETA1;
    reg_params.K_vertical_multiplier = PARAM_K_VERT_MULT;

    printf("  ✓ REGv1 parameters loaded from LoessPlateau.json\n");

    /* Initialize hydrology solver parameters */
    RichardsLiteParams hyd_params;
    hyd_params.K_r = 1.0e-4f;       /* Runoff layer conductivity */
    hyd_params.phi_r = 0.5f;        /* Runoff layer porosity */
    hyd_params.l_r = 0.005f;        /* 5mm runoff layer */
    hyd_params.b_T = 1.5f;          /* Transmissivity feedback */
    hyd_params.E_bare_ref = 5.0e-7f; /* ~4mm/day evaporation */
    hyd_params.dt_max = 3600.0f;    /* 1 hour max timestep */
    hyd_params.CFL_factor = 0.5f;   /* 50% of CFL limit */
    hyd_params.picard_tol = 1.0e-4f;
    hyd_params.picard_max_iter = 20;
    hyd_params.use_free_drainage = 1;

    printf("  ✓ HYD-RLv1 parameters configured\n");

    /* ====================================================================
     * STEP 3: Print Initial State
     * ==================================================================== */

    printf("\nStep 3: Initial state (sample cells):\n");
    print_cell_state(&grid[0], 0, 0);
    print_cell_state(&grid[16 * GRID_SIZE + 16], 16, 16);
    print_cell_state(&grid[31 * GRID_SIZE + 31], 31, 31);

    /* ====================================================================
     * STEP 4: Run Simulation Loop with Benchmarking
     * ==================================================================== */

    printf("\nStep 4: Running simulation (%d timesteps)...\n", NUM_STEPS);

    const float dt_hyd = 3600.0f;     /* 1 hour timestep for hydrology */
    const float dt_reg = 1.0f;        /* 1 year timestep for regeneration */
    const float rainfall = 1.0e-7f;   /* Light rainfall ~8mm/day */

    uint64_t start_time = get_time_ns();

    for (int step = 0; step < NUM_STEPS; step++) {
        /* Call hydrology solver every step */
        richards_lite_step(
            grid,
            &hyd_params,
            GRID_SIZE,  /* nx */
            GRID_SIZE,  /* ny */
            1,          /* nz = 1 layer for now */
            dt_hyd,
            rainfall,
            NULL        /* No diagnostics for now */
        );

        /* Call regeneration solver every 128 steps */
        if (step % REG_CALL_INTERVAL == 0) {
            regeneration_cascade_step(
                grid,
                NUM_CELLS,
                &reg_params,
                dt_reg
            );
        }

        /* Progress indicator */
        if ((step + 1) % 10 == 0) {
            printf("  Progress: %d/%d steps\n", step + 1, NUM_STEPS);
        }
    }

    uint64_t end_time = get_time_ns();
    uint64_t elapsed_ns = end_time - start_time;

    printf("  ✓ Simulation complete\n");

    /* ====================================================================
     * STEP 5: Print Final State and Verify Changes
     * ==================================================================== */

    printf("\nStep 5: Final state (sample cells):\n");
    print_cell_state(&grid[0], 0, 0);
    print_cell_state(&grid[16 * GRID_SIZE + 16], 16, 16);
    print_cell_state(&grid[31 * GRID_SIZE + 31], 31, 31);

    /* Verify that physics changed the state */
    printf("\nVerification:\n");
    int theta_changed = fabs(grid[0].theta - INITIAL_THETA) > 1e-6f;
    int veg_changed = fabs(grid[0].vegetation_cover - INITIAL_VEG_COVER) > 1e-6f;

    printf("  Theta changed:      %s (Δ = %.6f)\n",
           theta_changed ? "✓ YES" : "✗ NO",
           grid[0].theta - INITIAL_THETA);
    printf("  Vegetation changed: %s (Δ = %.6f)\n",
           veg_changed ? "✓ YES" : "✗ NO",
           grid[0].vegetation_cover - INITIAL_VEG_COVER);

    if (!theta_changed && !veg_changed) {
        printf("\n⚠ WARNING: Physics did not modify state! Check solver implementation.\n");
    }

    /* ====================================================================
     * STEP 6: Performance Analysis
     * ==================================================================== */

    printf("\n========================================================================\n");
    printf("PERFORMANCE RESULTS\n");
    printf("========================================================================\n");

    double elapsed_ms = elapsed_ns / 1.0e6;
    double elapsed_s = elapsed_ns / 1.0e9;

    printf("\nRaw Timing:\n");
    printf("  Total time:       %.3f ms (%.6f s)\n", elapsed_ms, elapsed_s);
    printf("  Timesteps:        %d\n", NUM_STEPS);
    printf("  Cells per step:   %d\n", NUM_CELLS);
    printf("  Total cell-steps: %llu\n", (unsigned long long)NUM_CELLS * NUM_STEPS);

    /* Calculate ns/cell/step (the critical metric) */
    double ns_per_cell_step = (double)elapsed_ns / (double)(NUM_CELLS * NUM_STEPS);

    printf("\nCritical Metric:\n");
    printf("  ns/cell/step:     %.2f ns\n", ns_per_cell_step);

    /* Decision matrix evaluation */
    printf("\nDecision Matrix Evaluation:\n");
    if (ns_per_cell_step < 150.0) {
        printf("  ✓ EXCELLENT (< 150 ns/cell)\n");
        printf("  Decision: Proceed immediately to visualization (GEO-v1 Sprint)\n");
    } else if (ns_per_cell_step < 250.0) {
        printf("  🟡 ACCEPTABLE (150-250 ns/cell)\n");
        printf("  Decision: Proceed to visualization, plan Doom Engine optimization\n");
    } else {
        printf("  ✗ PROBLEM (> 250 ns/cell)\n");
        printf("  Decision: HALT visualization, trigger Doom Engine optimization NOW\n");
    }

    /* Throughput metrics */
    double steps_per_second = NUM_STEPS / elapsed_s;
    double cells_per_second = (NUM_CELLS * NUM_STEPS) / elapsed_s;
    double megacells_per_second = cells_per_second / 1.0e6;

    printf("\nThroughput:\n");
    printf("  Steps/second:     %.2f\n", steps_per_second);
    printf("  Cells/second:     %.2e\n", cells_per_second);
    printf("  Megacells/second: %.3f\n", megacells_per_second);

    /* ====================================================================
     * CLEANUP
     * ==================================================================== */

    free(grid);

    printf("\n========================================================================\n");
    printf("✓ BENCHMARK COMPLETE\n");
    printf("========================================================================\n");

    return 0;
}
