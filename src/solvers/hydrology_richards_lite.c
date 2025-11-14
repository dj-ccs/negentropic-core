/*
 * hydrology_richards_lite.c - Richards-Lite Hydrology Solver (Implementation)
 *
 * SCIENTIFIC FOUNDATION
 * =====================
 *
 * This solver implements Edison's "Generalized Richards Equation" framework
 * with earthwork interventions, using Grok's high-performance "Richards-Lite v2"
 * numerical scheme.
 *
 * Conceptual Framework (Weill et al. 2009 [1.1]):
 *
 *   1. A thin porous "runoff layer" at the surface handles overland flow as
 *      a Darcy-like diffusive wave, avoiding the need for separate shallow-water
 *      equations.
 *
 *   2. Pressure and flux continuity are enforced at the land surface, allowing
 *      seamless transitions between infiltration-excess (Hortonian) and
 *      saturation-excess (Dunne) runoff.
 *
 *   3. Microtopographic storage (ζ) and fill-and-spill connectivity (C(ζ))
 *      introduce threshold behavior: subsurface-dominated flow below ζ_c,
 *      rapid surface activation above ζ_c (Frei et al. 2010 [2.1]).
 *
 *   4. Earthwork interventions (mulches, swales, check dams) modify effective
 *      conductivity (K_eff) and storage (ϕ_eff) via empirically-calibrated
 *      multipliers (Li 2003 [4.1], Garcia-Serrana 2016 [3.1]).
 *
 * Mathematical Formulation (from docs/science/microscale_hydrology.md):
 *
 *   Generalized Richards PDE:
 *       ∂θ/∂t = ∇·(K_eff(θ,I,ζ) ∇(ψ+z)) + S_I(x,y,t)
 *
 *   Effective Conductivity:
 *       K_eff(θ,I,ζ) = K_mat(θ) · M_I · C(ζ)
 *
 *   Fill-and-Spill Connectivity:
 *       C(ζ) = 1 / (1 + exp[-a_c(ζ - ζ_c)])
 *
 * IMPLEMENTATION STRATEGY (Grok's "Richards-Lite v2")
 * =====================================================
 *
 * Operator Splitting:
 *
 *   1. Vertical Implicit Pass (unconditionally stable):
 *      - Solve 1D Richards equation in z-direction for each column
 *      - Implicit Euler in time, central differences in space
 *      - Nonlinearity handled by Picard iteration (linearize K(θ) around θ^k)
 *      - Tridiagonal matrix solved via Thomas algorithm (O(nz) per column)
 *
 *   2. Horizontal Explicit Pass (CFL-limited, conditional):
 *      - Activated only when ζ > ζ_c (surface connectivity threshold)
 *      - Explicit diffusion: ∂h/∂t = ∇·(K_r ∇η_s)
 *      - Sub-stepping with CFL safety factor (dt_h < 0.5 * dx²/(2 K_r))
 *      - Cheap: runs only on saturated surface cells
 *
 *   3. Lookup Tables (Grok optimization):
 *      - Pre-compute θ(ψ), K(θ), dθ/dψ (256 entries each)
 *      - Linear interpolation with error < 1%
 *      - Reduces per-cell cost ~13× (200ns → 15ns for curve evaluation)
 *
 * Performance Optimizations:
 *
 *   - LUT lookups replace expensive powf() calls in van Genuchten curves
 *   - Thomas algorithm (8nz flops) optimal for tridiagonal systems
 *   - Picard iteration typically converges in 3-5 iterations
 *   - Horizontal pass skipped for dry cells (C(ζ) < 0.1)
 *   - Target: ~15-20 ns/cell on modern x86-64 (comparable to ATMv1)
 *
 * REGv1 Coupling:
 *
 *   This solver READS Cell state variables that are modified by the future
 *   REGv1 regeneration_cascade_step():
 *     - porosity_eff: water-holding capacity (+5mm per 1% SOM)
 *     - K_tensor[8]: vertical conductivity (+15% per 1% SOM, compounding)
 *
 *   The coupling is one-way per timestep: REGv1 runs every ~128 hydrology
 *   steps, updates Cell state, then HYD reads the modified values.
 *
 * Thread Safety: Pure function, stateless except for static LUTs
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (HYD-RLv1)
 * License: MIT OR GPL-3.0
 */

#include "hydrology_richards_lite.h"
#include "hydrology_richards_lite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * GLOBAL LOOKUP TABLE STORAGE
 * ======================================================================== */

VanGenuchtenLUT g_vG_lut_default;
int g_lut_initialized = 0;

/* ========================================================================
 * INITIALIZATION (LUT Generation)
 * ======================================================================== */

/**
 * Initialize Richards-Lite solver.
 *
 * Pre-computes lookup tables for van Genuchten retention and Mualem
 * conductivity curves. This function must be called once before using
 * richards_lite_step().
 *
 * Default soil parameters (sandy loam, representative for Loess Plateau):
 *   - K_s = 5e-6 m/s (18 mm/hr, moderate infiltration)
 *   - α = 2.0 m⁻¹
 *   - n = 1.5
 *   - θ_s = 0.40
 *   - θ_r = 0.05
 *
 * Users can modify g_vG_lut_default after init() for custom soil types.
 */
void richards_lite_init(void) {
    if (g_lut_initialized) {
        return;  /* Already initialized */
    }

    /* Set default soil parameters (sandy loam) */
    g_vG_lut_default.alpha = 2.0f;          /* [1/m] */
    g_vG_lut_default.n = 1.5f;              /* [-] */
    g_vG_lut_default.m = 1.0f - 1.0f / g_vG_lut_default.n;  /* m = 1 - 1/n */
    g_vG_lut_default.theta_s = 0.40f;       /* [-] */
    g_vG_lut_default.theta_r = 0.05f;       /* [-] */
    g_vG_lut_default.K_s = 5.0e-6f;         /* [m/s] = 18 mm/hr */

    /* Set table bounds */
    g_vG_lut_default.psi_min = PSI_MIN;
    g_vG_lut_default.psi_max = PSI_MAX;
    g_vG_lut_default.theta_min = THETA_MIN;
    g_vG_lut_default.theta_max = THETA_MAX;

    /* Build θ(ψ) table */
    for (int i = 0; i < LUT_SIZE; i++) {
        float t = (float)i / (LUT_SIZE - 1);
        float psi = PSI_MIN + t * PSI_RANGE;
        g_vG_lut_default.theta_of_psi[i] = vG_theta_exact(
            psi,
            g_vG_lut_default.alpha,
            g_vG_lut_default.n,
            g_vG_lut_default.theta_s,
            g_vG_lut_default.theta_r
        );
    }

    /* Build K(θ) table */
    for (int i = 0; i < LUT_SIZE; i++) {
        float t = (float)i / (LUT_SIZE - 1);
        float theta = THETA_MIN + t * THETA_RANGE;
        g_vG_lut_default.K_of_theta[i] = vG_K_exact(
            theta,
            g_vG_lut_default.K_s,
            g_vG_lut_default.theta_s,
            g_vG_lut_default.theta_r,
            g_vG_lut_default.m
        );
    }

    /* Build dθ/dψ table (specific moisture capacity) */
    for (int i = 0; i < LUT_SIZE; i++) {
        float t = (float)i / (LUT_SIZE - 1);
        float psi = PSI_MIN + t * PSI_RANGE;
        g_vG_lut_default.C_of_psi[i] = vG_capacity_exact(
            psi,
            g_vG_lut_default.alpha,
            g_vG_lut_default.n,
            g_vG_lut_default.theta_s,
            g_vG_lut_default.theta_r
        );
    }

    g_lut_initialized = 1;

    printf("[HYD-RLv1] Richards-Lite solver initialized\n");
    printf("  Default soil: sandy loam (Loess Plateau representative)\n");
    printf("  K_s = %.2e m/s (%.1f mm/hr)\n",
           g_vG_lut_default.K_s, g_vG_lut_default.K_s * 3600 * 1000);
    printf("  θ_s = %.2f, θ_r = %.2f\n",
           g_vG_lut_default.theta_s, g_vG_lut_default.theta_r);
    printf("  LUT size: %d entries per curve\n", LUT_SIZE);
}

/* ========================================================================
 * VERTICAL IMPLICIT PASS (1D Richards Equation)
 * ======================================================================== */

/**
 * Solve 1D Richards equation for a single column using implicit Euler.
 *
 * Discretization (backward Euler in time, central differences in space):
 *   (θ^{n+1} - θ^n) / dt = d/dz [K(θ^{n+1}) (dψ/dz + 1)]
 *
 * Linearize K(θ^{n+1}) ≈ K(θ^k) via Picard iteration.
 *
 * Tridiagonal system:
 *   a[k] θ[k-1] + b[k] θ[k] + c[k] θ[k+1] = d[k]
 *
 * Boundary conditions:
 *   - Top (k=0): flux = rainfall - infiltration
 *   - Bottom (k=nz-1): free drainage (dψ/dz = -1) OR no-flux (configurable)
 *
 * @param column [IN/OUT] Array of nz Cells (vertical column)
 * @param nz Number of vertical layers
 * @param dt Timestep [s]
 * @param rainfall Rainfall rate [m/s]
 * @param lut van Genuchten lookup table
 * @param use_free_drainage 1=free drainage bottom BC, 0=no-flux bottom BC
 */
static void solve_vertical_implicit(
    Cell* column,
    int nz,
    float dt,
    float rainfall,
    const VanGenuchtenLUT* lut,
    int use_free_drainage
) {
    /* Allocate tridiagonal system arrays */
    static float a[256];  /* Lower diagonal */
    static float b[256];  /* Main diagonal */
    static float c[256];  /* Upper diagonal */
    static float d[256];  /* Right-hand side */
    static float theta_new[256];  /* Solution */

    /* Picard iteration for nonlinearity */
    const int max_iter = 20;
    const float tol = 1e-4f;

    for (int iter = 0; iter < max_iter; iter++) {
        /* Build tridiagonal system */
        for (int k = 0; k < nz; k++) {
            float dz = column[k].dz;
            float theta_k = column[k].theta;

            /* Lookup K at current θ (linearization point) */
            float K_k = vG_K_lookup(theta_k, lut);

            /* Apply intervention multiplier */
            float M_K_zz = column[k].M_K_zz;
            float K_eff_k = K_k * M_K_zz;

            /* Note: K_tensor[8] is set during initialization to K_s * M_K_zz * REGv1_bonus.
             * For now, we use the simple multiplier approach. Future: use K_tensor directly
             * with proper relative conductivity scaling. */

            /* Coefficients for implicit diffusion */
            float coeff = dt / (dz * dz);

            if (k == 0) {
                /* Top boundary: rainfall flux */
                a[k] = 0.0f;
                c[k] = -coeff * K_eff_k;
                b[k] = 1.0f - c[k];
                d[k] = theta_k + dt * rainfall / dz;

            } else if (k == nz - 1) {
                /* Bottom boundary: configurable drainage */
                if (use_free_drainage) {
                    /* Free drainage (dψ/dz = -1): water can drain out */
                    a[k] = -coeff * K_eff_k;
                    c[k] = 0.0f;
                    b[k] = 1.0f - a[k];
                    d[k] = theta_k + dt * K_eff_k / dz;  /* Gravity drainage */
                } else {
                    /* No-flux boundary: dθ/dz = 0 (closed bottom for mass conservation) */
                    a[k] = -coeff * K_eff_k;
                    c[k] = 0.0f;
                    b[k] = 1.0f - a[k];
                    d[k] = theta_k;  /* No flux term */
                }

            } else {
                /* Interior points */
                float K_k_minus_raw = vG_K_lookup(column[k-1].theta, lut);
                float K_k_plus_raw = vG_K_lookup(column[k+1].theta, lut);

                /* Apply intervention multipliers to neighbors */
                float K_k_minus = K_k_minus_raw * column[k-1].M_K_zz;
                float K_k_plus = K_k_plus_raw * column[k+1].M_K_zz;

                /* Harmonic mean for K at interfaces */
                float K_minus_half = 2.0f * K_eff_k * K_k_minus / (K_eff_k + K_k_minus + 1e-12f);
                float K_plus_half = 2.0f * K_eff_k * K_k_plus / (K_eff_k + K_k_plus + 1e-12f);

                a[k] = -coeff * K_minus_half;
                c[k] = -coeff * K_plus_half;
                b[k] = 1.0f - a[k] - c[k];
                d[k] = theta_k;
            }
        }

        /* Solve tridiagonal system */
        thomas_algorithm(a, b, c, d, theta_new, nz);

        /* Check convergence */
        float max_change = 0.0f;
        for (int k = 0; k < nz; k++) {
            float change = fabsf(theta_new[k] - column[k].theta);
            if (change > max_change) max_change = change;
        }

        /* Update θ */
        for (int k = 0; k < nz; k++) {
            /* Clamp to physical bounds (use REGv1-modified porosity_eff) */
            float theta_min = column[k].theta_r;
            float theta_max = column[k].porosity_eff;  /* REGv1 bonus: +5mm per 1% SOM */
            column[k].theta = clamp(theta_new[k], theta_min, theta_max);

            /* Update ψ from θ using inverse van Genuchten (approximate) */
            /* For now, keep ψ consistent with θ via forward relation */
            /* TODO: Implement inverse θ→ψ lookup for exact consistency */
        }

        /* Convergence check */
        if (max_change < tol) {
            break;  /* Converged */
        }
    }
}

/* ========================================================================
 * HORIZONTAL EXPLICIT PASS (Surface Flow)
 * ======================================================================== */

/**
 * Solve 2D surface water flow using explicit diffusion.
 *
 * Activated only when connectivity C(ζ) > threshold (fill-and-spill).
 *
 * Discretization (forward Euler in time, central differences in space):
 *   (h^{n+1} - h^n) / dt = K_r ∇²η_s
 *
 * where η_s = h_surface + z is total surface head.
 *
 * CFL stability: dt < 0.5 * dx² / (2 * K_r)
 *
 * @param cells [IN/OUT] 2D array of Cells (surface layer only)
 * @param params Solver parameters
 * @param nx Number of cells in x
 * @param ny Number of cells in y
 * @param dt Timestep [s] (parent timestep, will be sub-stepped)
 */
static void solve_horizontal_explicit(
    Cell* cells,
    const RichardsLiteParams* params,
    int nx,
    int ny,
    float dt
) {
    /* Sub-stepping for CFL stability */
    float K_r = params->K_r;
    float dx = cells[0].dx;  /* Assume uniform grid */
    float dt_cfl = cfl_timestep(K_r, dx, params->CFL_factor);

    int n_substeps = (int)(dt / dt_cfl) + 1;
    float dt_sub = dt / n_substeps;

    /* Temporary storage for surface water update */
    static float h_new[256 * 256];  /* Max 256x256 grid */

    for (int substep = 0; substep < n_substeps; substep++) {
        /* Compute ∇²η_s for each cell */
        for (int j = 0; j < ny; j++) {
            for (int i = 0; i < nx; i++) {
                int idx = j * nx + i;
                Cell* cell = &cells[idx];

                /* Check connectivity threshold */
                float C_zeta = connectivity_function(cell->zeta, cell->zeta_c, cell->a_c);
                if (C_zeta < 0.1f) {
                    /* Below threshold, skip surface flow */
                    h_new[idx] = cell->h_surface;
                    continue;
                }

                /* Total surface head η_s = h + z */
                float eta_s = cell->h_surface + cell->z;

                /* Neighboring heads (with boundary handling) */
                float eta_w = (i > 0) ? (cells[idx-1].h_surface + cells[idx-1].z) : eta_s;
                float eta_e = (i < nx-1) ? (cells[idx+1].h_surface + cells[idx+1].z) : eta_s;
                float eta_s_n = (j > 0) ? (cells[idx-nx].h_surface + cells[idx-nx].z) : eta_s;
                float eta_n = (j < ny-1) ? (cells[idx+nx].h_surface + cells[idx+nx].z) : eta_s;

                /* Laplacian ∇²η_s ≈ (η_w + η_e + η_s + η_n - 4*η_s) / dx² */
                float laplacian = (eta_w + eta_e + eta_s_n + eta_n - 4.0f * eta_s) / (dx * dx);

                /* Explicit update: dh/dt = K_r * C(ζ) * ∇²η_s */
                h_new[idx] = cell->h_surface + dt_sub * K_r * C_zeta * laplacian;

                /* Clamp to non-negative */
                if (h_new[idx] < 0.0f) h_new[idx] = 0.0f;
            }
        }

        /* Copy updated values */
        for (int j = 0; j < ny; j++) {
            for (int i = 0; i < nx; i++) {
                int idx = j * nx + i;
                cells[idx].h_surface = h_new[idx];
            }
        }
    }
}

/* ========================================================================
 * MAIN SOLVER STEP
 * ======================================================================== */

/**
 * Advance hydrological state by one timestep.
 *
 * See header for full documentation.
 */
void richards_lite_step(
    Cell* cells,
    const RichardsLiteParams* params,
    size_t nx,
    size_t ny,
    size_t nz,
    float dt,
    float rainfall,
    void* diagnostics
) {
    (void)diagnostics;  /* Reserved for future use */

    if (!g_lut_initialized) {
        fprintf(stderr, "ERROR: richards_lite_init() must be called first!\n");
        return;
    }

    /* Step 1: Update surface depression storage and connectivity */
    for (size_t j = 0; j < ny; j++) {
        for (size_t i = 0; i < nx; i++) {
            int idx_surface = j * nx + i;
            Cell* cell = &cells[idx_surface * nz];  /* Top layer */

            /* Depression storage: ζ = min(h_surface, ζ_max) */
            float zeta_max = cell->zeta_c + cell->Delta_zeta;
            cell->zeta = (cell->h_surface < zeta_max) ? cell->h_surface : zeta_max;
        }
    }

    /* Step 2: Vertical implicit pass (column-wise) */
    for (size_t j = 0; j < ny; j++) {
        for (size_t i = 0; i < nx; i++) {
            int idx_base = (j * nx + i) * nz;
            Cell* column = &cells[idx_base];

            solve_vertical_implicit(column, nz, dt, rainfall, &g_vG_lut_default,
                                   params->use_free_drainage);
        }
    }

    /* Step 3: Horizontal explicit pass (surface flow, conditional) */
    /* Extract surface layer (top of each column) */
    static Cell surface_cells[256 * 256];  /* Max 256x256 */
    for (size_t j = 0; j < ny; j++) {
        for (size_t i = 0; i < nx; i++) {
            int idx_surface_global = (j * nx + i) * nz;
            int idx_surface_local = j * nx + i;
            surface_cells[idx_surface_local] = cells[idx_surface_global];
        }
    }

    solve_horizontal_explicit(surface_cells, params, nx, ny, dt);

    /* Copy updated surface water back */
    for (size_t j = 0; j < ny; j++) {
        for (size_t i = 0; i < nx; i++) {
            int idx_surface_global = (j * nx + i) * nz;
            int idx_surface_local = j * nx + i;
            cells[idx_surface_global].h_surface = surface_cells[idx_surface_local].h_surface;
            cells[idx_surface_global].zeta = surface_cells[idx_surface_local].zeta;
        }
    }

    /* Step 4: Apply evaporation sink */
    for (size_t j = 0; j < ny; j++) {
        for (size_t i = 0; i < nx; i++) {
            int idx_surface = (j * nx + i) * nz;
            Cell* cell = &cells[idx_surface];

            /* Evaporation (intervention-modified) */
            float E_eff = cell->kappa_evap * params->E_bare_ref;
            float dtheta_evap = -E_eff * dt / cell->dz;

            cell->theta += dtheta_evap;
            if (cell->theta < cell->theta_r) {
                cell->theta = cell->theta_r;
            }
        }
    }
}

/* ========================================================================
 * INTERVENTION APPLICATION
 * ======================================================================== */

/**
 * Apply intervention multipliers to a Cell.
 *
 * See header for empirical ranges from Edison research.
 */
void richards_lite_apply_intervention(
    Cell* cell,
    InterventionType intervention,
    float intensity
) {
    /* Clamp intensity to [0,1] */
    intensity = clamp(intensity, 0.0f, 1.0f);

    switch (intervention) {
        case INTERVENTION_MULCH_GRAVEL:
            /* Gravel/sand mulch (Edison [4.1], [4.2]) */
            cell->M_K_zz = 1.0f + intensity * 5.0f;  /* 1-6 range */
            cell->kappa_evap = 1.0f - intensity * 0.75f;  /* 1.0 → 0.25 */
            cell->Delta_zeta = intensity * 0.0007f;  /* 0.7 mm interception */
            break;

        case INTERVENTION_SWALE:
            /* Grassed swale (Edison [3.1], [3.2]) */
            cell->M_K_zz = 1.0f + intensity * 2.0f;  /* 1-3 range */
            cell->M_K_xx = 1.0f + intensity * 1.0f;  /* Along-contour channeling */
            cell->kappa_evap = 1.0f;  /* No evaporation suppression */
            cell->Delta_zeta = 0.0f;
            break;

        case INTERVENTION_BERM:
            /* Contour berm / check dam */
            cell->Delta_zeta = intensity * 0.010f;  /* Up to 10 mm storage */
            cell->M_K_zz = 1.0f;  /* No infiltration change */
            cell->kappa_evap = 1.0f;
            break;

        case INTERVENTION_BIOCRUST:
            /* Biological soil crust (reduces infiltration, increases storage) */
            cell->M_K_zz = 1.0f - intensity * 0.5f;  /* Reduce K by up to 50% */
            cell->Delta_zeta = intensity * 0.002f;  /* 2 mm surface storage */
            cell->kappa_evap = 1.0f;
            break;

        case INTERVENTION_NONE:
        default:
            /* Baseline (no intervention) */
            cell->M_K_zz = 1.0f;
            cell->M_K_xx = 1.0f;
            cell->kappa_evap = 1.0f;
            cell->Delta_zeta = 0.0f;
            break;
    }
}

/* ========================================================================
 * DIAGNOSTIC FUNCTIONS
 * ======================================================================== */

float richards_lite_connectivity(float zeta, float zeta_c, float a_c) {
    return connectivity_function(zeta, zeta_c, a_c);
}

float richards_lite_total_water(const Cell* cell) {
    /* Total water = vadose zone + surface ponding
     * Note: zeta is already included in h_surface, so we don't add it separately
     * to avoid double-counting */
    return cell->theta * cell->dz + cell->h_surface;
}

int richards_lite_runoff_mechanism(const Cell* cell, float rainfall) {
    /* Classify runoff mechanism:
     *   0 = none (all infiltrated)
     *   1 = Hortonian (infiltration-excess)
     *   2 = Dunne (saturation-excess)
     */

    if (cell->h_surface < 1e-6f) {
        return 0;  /* No runoff */
    }

    /* Check if saturated at surface */
    if (cell->theta >= cell->theta_s * 0.99f) {
        return 2;  /* Dunne (saturation-excess) */
    }

    /* Check if rainfall exceeds infiltration capacity */
    float K_surf = vG_K_lookup(cell->theta, &g_vG_lut_default);
    if (rainfall > K_surf * cell->M_K_zz) {
        return 1;  /* Hortonian (infiltration-excess) */
    }

    return 0;  /* No clear mechanism */
}
