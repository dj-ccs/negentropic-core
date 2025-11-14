# REGv1 Integration Example

## Overview

This document provides a concrete example of how to integrate the REGv1 Regeneration Cascade Solver with the HYD-RLv1 hydrology solver in a main simulation loop.

## Key Integration Requirements

1. **Execution Frequency**: REGv1 must be called exactly once every **128 hydrology steps**
2. **Performance Budget**: REGv1 execution must stay within **5% of total frame time**
3. **State Synchronization**: Cell struct fields must be properly initialized and maintained
4. **Parameter Loading**: Parameters must be loaded once at initialization

## Integration Pattern

### Initialization Phase

```c
#include "hydrology_richards_lite.h"
#include "regeneration_cascade.h"

/* Global parameters (loaded once at startup) */
RegenerationParams reg_params;

int simulation_init(const char* param_file) {
    /* Load regeneration parameters */
    if (regeneration_cascade_load_params(param_file, &reg_params) != 0) {
        fprintf(stderr, "Failed to load regeneration parameters\n");
        return -1;
    }

    /* Initialize hydrology solver */
    richards_lite_init();

    return 0;
}
```

### Cell Initialization

```c
void initialize_cell(Cell* cell) {
    /* Hydrological initial conditions */
    cell->theta = 0.15f;              /* Initial moisture [m³/m³] */
    cell->psi = -10.0f;               /* Initial matric head [m] */
    cell->h_surface = 0.0f;           /* No initial ponding */
    cell->zeta = 0.0f;                /* No depression storage */

    /* Soil parameters */
    cell->K_s = 1e-5f;                /* Saturated conductivity [m/s] */
    cell->alpha_vG = 2.0f;            /* van Genuchten alpha [1/m] */
    cell->n_vG = 1.5f;                /* van Genuchten n [-] */
    cell->theta_s = 0.45f;            /* Saturated water content */
    cell->theta_r = 0.05f;            /* Residual water content */

    /* REGv1 initial conditions (degraded state) */
    cell->vegetation_cover = 0.15f;   /* 15% cover (degraded) */
    cell->SOM_percent = 0.5f;         /* 0.5% SOM (very low) */

    /* Initialize fixed-point versions */
    cell->vegetation_cover_fxp = FLOAT_TO_FXP(cell->vegetation_cover);
    cell->SOM_percent_fxp = FLOAT_TO_FXP(cell->SOM_percent);

    /* Initialize coupling parameters (modified by REGv1) */
    cell->porosity_eff = cell->theta_s;  /* Start with base porosity */

    /* Initialize K_tensor (isotropic for simplicity) */
    for (int i = 0; i < 9; i++) {
        cell->K_tensor[i] = 0.0f;
    }
    cell->K_tensor[0] = cell->K_s;    /* K_xx */
    cell->K_tensor[4] = cell->K_s;    /* K_yy */
    cell->K_tensor[8] = cell->K_s;    /* K_zz (vertical) */

    /* Grid geometry */
    cell->z = 0.0f;                   /* Surface elevation */
    cell->dz = 0.10f;                 /* 10 cm layer thickness */
    cell->dx = 10.0f;                 /* 10 m horizontal spacing */
}
```

### Main Simulation Loop

```c
void run_simulation(Cell* grid, size_t nx, size_t ny, size_t nz, int n_years) {
    /* Calculate grid size for REGv1 (only top layer, so nx * ny) */
    size_t grid_size_2d = nx * ny;
    size_t grid_size_3d = nx * ny * nz;

    /* Time parameters */
    float dt_hydrology = 3600.0f;     /* 1 hour timestep for hydrology [s] */
    float dt_reg_years = 1.0f;        /* 1 year timestep for REGv1 [years] */

    /* Conversion: how many hydrology steps per year? */
    /* 365 days/year * 24 hours/day = 8760 hours/year */
    int hydro_steps_per_year = 8760;

    /* REGv1 frequency: once every 128 hydrology steps */
    int REG_CALL_FREQUENCY = 128;

    /* Hydrology solver parameters */
    RichardsLiteParams hyd_params;
    hyd_params.K_r = 1e-4f;
    hyd_params.phi_r = 0.5f;
    hyd_params.l_r = 0.005f;
    hyd_params.b_T = 1.0f;
    hyd_params.E_bare_ref = 1e-7f;
    hyd_params.dt_max = dt_hydrology;
    hyd_params.CFL_factor = 0.5f;
    hyd_params.picard_tol = 1e-4f;
    hyd_params.picard_max_iter = 20;
    hyd_params.use_free_drainage = 1;

    /* Main time loop */
    int hydro_step_counter = 0;
    int total_hydro_steps = n_years * hydro_steps_per_year;

    printf("Starting simulation: %d years = %d hydrology steps\n",
           n_years, total_hydro_steps);
    printf("REGv1 will be called every %d steps (~%.1f times per year)\n",
           REG_CALL_FREQUENCY, (float)hydro_steps_per_year / REG_CALL_FREQUENCY);

    for (int step = 0; step < total_hydro_steps; step++) {
        /* Hydrology step (fast timescale) */
        float rainfall = 0.0f;  /* [m/s] - could be time-varying */
        richards_lite_step(grid, &hyd_params, nx, ny, nz,
                          dt_hydrology, rainfall, NULL);

        /* Increment counter */
        hydro_step_counter++;

        /* Regeneration step (slow timescale) - every 128 hydrology steps */
        if (hydro_step_counter == REG_CALL_FREQUENCY) {
            /* Call REGv1 on the top layer only */
            /* Note: REGv1 operates on 2D grid (nx × ny), not full 3D */
            regeneration_cascade_step(grid, grid_size_2d, &reg_params, dt_reg_years);

            /* Reset counter */
            hydro_step_counter = 0;

            /* Optional: Print diagnostic info */
            if (step % (hydro_steps_per_year) == 0) {
                int year = step / hydro_steps_per_year;
                Cell* sample = &grid[nx/2 + (ny/2)*nx];  /* Center cell */
                printf("Year %d: V=%.3f, SOM=%.3f%%, θ=%.3f, K_zz=%.2e\n",
                       year,
                       sample->vegetation_cover,
                       sample->SOM_percent,
                       sample->theta,
                       sample->K_tensor[8]);
            }
        }
    }

    printf("Simulation complete.\n");
}
```

### Example Main Function

```c
int main(int argc, char** argv) {
    /* Parse command line */
    const char* param_file = "config/parameters/LoessPlateau.json";
    if (argc > 1) {
        param_file = argv[1];
    }

    /* Initialize */
    if (simulation_init(param_file) != 0) {
        return 1;
    }

    /* Create grid */
    size_t nx = 50;   /* 50 cells in x */
    size_t ny = 50;   /* 50 cells in y */
    size_t nz = 10;   /* 10 vertical layers */
    size_t grid_size = nx * ny * nz;

    Cell* grid = (Cell*)calloc(grid_size, sizeof(Cell));
    if (!grid) {
        fprintf(stderr, "Failed to allocate grid\n");
        return 1;
    }

    /* Initialize all cells */
    for (size_t i = 0; i < grid_size; i++) {
        initialize_cell(&grid[i]);
    }

    /* Run simulation for 20 years */
    run_simulation(grid, nx, ny, nz, 20);

    /* Cleanup */
    free(grid);

    return 0;
}
```

## Important Notes

### 1. Grid Structure

The grid is structured as a 1D array with 3D indexing:
```c
/* Access cell at (ix, iy, iz) */
size_t idx = ix + iy * nx + iz * (nx * ny);
Cell* cell = &grid[idx];
```

For REGv1, only the **top layer** (iz = 0) is processed, so you pass `grid` with size `nx * ny`.

### 2. State Synchronization

REGv1 modifies the following Cell fields that HYD-RLv1 reads:
- `vegetation_cover` and `vegetation_cover_fxp` (REGv1 updates both)
- `SOM_percent` and `SOM_percent_fxp` (REGv1 updates both)
- `porosity_eff` (REGv1 adds hydrological bonus)
- `K_tensor[8]` (REGv1 multiplies by compounding factor)

HYD-RLv1 reads these values every step, so the coupling is automatically maintained.

### 3. Performance Monitoring

To verify the 5% budget constraint:
```c
#include <time.h>

clock_t reg_start = clock();
regeneration_cascade_step(grid, grid_size_2d, &reg_params, dt_reg_years);
clock_t reg_end = clock();

double reg_time = (double)(reg_end - reg_start) / CLOCKS_PER_SEC;
double frame_time = dt_hydrology * REG_CALL_FREQUENCY;
double reg_fraction = reg_time / frame_time;

if (reg_fraction > 0.05) {
    fprintf(stderr, "WARNING: REGv1 exceeded 5%% budget (%.2f%%)\n",
            reg_fraction * 100.0);
}
```

### 4. Multi-Layer Moisture Averaging

For proper theta_avg calculation with multi-layer grids:
```c
/* Extract vertical profile for a column at (ix, iy) */
float theta_profile[3];
for (int iz = 0; iz < 3; iz++) {
    size_t idx = ix + iy * nx + iz * (nx * ny);
    theta_profile[iz] = grid[idx].theta;
}

/* Calculate weighted average */
float theta_avg = calculate_theta_avg(theta_profile, 3);
```

This would require modifying the `regeneration_cascade_step` function to accept a multi-layer grid and compute theta_avg internally. For v1.0, we use the simpler single-layer approach.

## Validation Checklist

Before deploying the integrated system:

- [ ] Parameters loaded successfully from JSON
- [ ] Cell struct fields properly initialized (especially fixed-point versions)
- [ ] REGv1 called exactly every 128 hydrology steps
- [ ] REGv1 execution time < 5% of frame budget
- [ ] Vegetation cover increases over time in degraded regions
- [ ] SOM increases with vegetation growth
- [ ] Porosity_eff increases measurably (hydrological bonus)
- [ ] K_tensor[8] increases measurably (hydrological bonus)
- [ ] Mass balance maintained in hydrology solver
- [ ] No numerical instabilities over long simulations

## Future Enhancements

For REGv2, consider:
1. **Multi-layer theta averaging**: Properly compute theta_avg from vertical profile
2. **Spatially-varying parameters**: Support parameter maps instead of global params
3. **Temperature modulation**: Add T-dependent SOM decay (a2 * f(T))
4. **Erosion coupling**: Link vegetation cover to sediment transport
5. **Nutrient cycles**: Extend to N, P cycling alongside SOM
6. **Stochastic forcing**: Add rainfall variability and extreme events
7. **Adaptive timestep**: Dynamically adjust REG_CALL_FREQUENCY based on state change rates

## References

- Edison Research: `docs/science/macroscale_regeneration.md`
- Grok Specification: Task description for REGv1 sprint
- HYD-RLv1 Documentation: `src/solvers/hydrology_richards_lite.h`
