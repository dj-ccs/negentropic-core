/*
 * atmosphere_biotic.c - Biotic Pump Atmospheric Solver (Implementation)
 *
 * SCIENTIFIC FOUNDATION
 * =====================
 *
 * This solver implements the Biotic Pump theory of condensation-induced
 * atmospheric dynamics (CIAD), which posits that forest evapotranspiration
 * drives horizontal pressure gradients and ocean-to-land winds through
 * the mechanism of vapor condensation.
 *
 * Conceptual Framework (Makarieva & Gorshkov 2010 [1.1]):
 *
 *   1. Forests maintain high evapotranspiration (ET) through leaf area
 *      and aerodynamic roughness, injecting water vapor into the atmosphere.
 *
 *   2. Rising moist air cools and condenses, creating a local pressure drop
 *      because condensation removes the vapor's partial pressure contribution.
 *
 *   3. The horizontal pressure gradient ∂p/∂x drives an inflow from ocean
 *      (low ET, high pressure) toward forest (high ET, low pressure).
 *
 *   4. This circulation imports moisture inland, sustaining precipitation
 *      far from coasts—behavior observed in intact forests but absent in
 *      deforested regions ([2.1], [3.1]).
 *
 * Mathematical Formulation (from Edison Specification):
 *
 *   Pressure gradient (Eq 10):
 *       ∂p/∂x = (h_γ/L) · (p_v / h_c)
 *
 *   Vapor partial pressure (Eq 11):
 *       p_v = r_T · e_s(T) · [RH_0 + k_E · E · φ(LAI, H_c)]
 *
 *   Momentum balance (Eq 12):
 *       ρ ∂u/∂t = -∂p/∂x - ρ c_d u|u| + ρ f v
 *       ρ ∂v/∂t = -ρ f u - ρ c_d v|v|
 *
 *   Continuity (implicit):
 *       u h_γ = w L   (links horizontal inflow to vertical motion)
 *
 * Key Parameters & Thresholds ([2.1], [3.1], [5.1]):
 *
 *   - Forest continuity φ_f: critical threshold ~0.6-0.7
 *     Below threshold: L ~ 6×10⁵ m, P(x) decays exponentially (e-folding ~600 km)
 *     Above threshold: L ~ 1-2×10⁶ m, P(x) sustained over >1000 km
 *
 *   - Leaf area index (LAI): minimum ~3-4 needed for sufficient ET
 *
 *   - Condensation efficiency α: highly sensitive (enters as γ_d^-1 ~ 10²)
 *
 * Empirical Validation ([3.1]):
 *
 *   Non-forested transects: ln P(x) = a + bx with |b|^-1 ~ 600 km
 *   Forested transects: P(x) nearly flat over continental scales
 *
 * IMPLEMENTATION STRATEGY
 * =======================
 *
 * Performance Optimizations (Grok):
 *
 *   1. Pre-computed Clausius-Clapeyron lookup table (256 entries)
 *      - Covers 243-333 K (-30°C to +60°C) with linear interpolation
 *      - Reduces e_s(T) cost from ~100 ns to ~5 ns per cell
 *
 *   2. Sqrt-free semi-implicit drag kernel
 *      - Approximates |u| ≈ max(|u|, |v|) + 0.5·min(|u|, |v|)
 *      - Error < 12% vs true sqrt, but ~3× faster
 *      - Semi-implicit update avoids CFL restriction
 *
 *   3. SIMD-ready structure
 *      - Aligned loops for future vectorization
 *      - Placeholders for WASM SIMD128
 *
 * Numerical Scheme:
 *
 *   - Pressure gradient: diagnosed from vegetation state (explicit)
 *   - Advection/Coriolis: explicit Euler (stable for large-scale flow)
 *   - Drag: semi-implicit (unconditionally stable)
 *
 * Thread Safety: Pure function, stateless except for static LUT
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (ATMv1)
 * License: MIT OR GPL-3.0
 */

#include "atmosphere_biotic.h"
#include <math.h>
#include <string.h>

/* ========================================================================
 * LOOKUP TABLE FOR SATURATION VAPOR PRESSURE
 * ======================================================================== */

#define LUT_SIZE 256
#define T_MIN 243.0f   /* -30°C in Kelvin */
#define T_MAX 333.0f   /* +60°C in Kelvin */
#define T_RANGE (T_MAX - T_MIN)

/* Pre-computed e_s(T) table (initialized by biotic_pump_init) */
static float g_e_s_lut[LUT_SIZE];
static int g_lut_initialized = 0;

/**
 * August-Roche-Magnus formula for saturation vapor pressure.
 *
 * e_s(T) = 611.2 * exp(17.67 * (T - 273.15) / (T - 29.65))  [Pa]
 *
 * Valid for 243 K < T < 333 K (accurate to ~0.1% vs full Clausius-Clapeyron).
 *
 * Reference: Alduchov & Eskridge (1996), improved Magnus formula
 */
static float compute_e_s_exact(float T_kelvin) {
    const float T_celsius = T_kelvin - 273.15f;
    const float numerator = 17.67f * T_celsius;
    const float denominator = T_celsius + 243.5f;
    return 611.2f * expf(numerator / denominator);
}

void biotic_pump_init(void) {
    if (g_lut_initialized) return;

    /* Fill lookup table */
    for (int i = 0; i < LUT_SIZE; i++) {
        float T = T_MIN + (T_RANGE * i) / (LUT_SIZE - 1);
        g_e_s_lut[i] = compute_e_s_exact(T);
    }

    g_lut_initialized = 1;
}

float biotic_pump_saturation_vapor_pressure(float T_kelvin) {
    /* Clamp to valid range */
    if (T_kelvin < T_MIN) T_kelvin = T_MIN;
    if (T_kelvin > T_MAX) T_kelvin = T_MAX;

    /* Linear interpolation in LUT */
    float t_norm = (T_kelvin - T_MIN) / T_RANGE;
    float index_f = t_norm * (LUT_SIZE - 1);
    int i0 = (int)index_f;
    int i1 = i0 + 1;
    if (i1 >= LUT_SIZE) i1 = LUT_SIZE - 1;

    float frac = index_f - i0;
    return g_e_s_lut[i0] * (1.0f - frac) + g_e_s_lut[i1] * frac;
}

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/**
 * Compute inland length scale L(phi_f) from forest continuity.
 *
 * Empirical power-law fit to observed precipitation-distance behavior
 * ([2.1], [3.1]):
 *
 *   L(phi_f) = L_min + (L_max - L_min) * phi_f^beta
 *
 * Parameters:
 *   L_min = 6e5 m      (non-forested e-folding)
 *   L_max = 2e6 m      (maximum forested fetch)
 *   beta = 2.5         (threshold exponent; rapid transition near 0.6-0.7)
 *
 * Physical interpretation:
 *   - phi_f = 0 (no forest): L = 600 km, P decays exponentially
 *   - phi_f = 1 (continuous forest): L = 2000 km, P sustained
 *   - phi_f ~ 0.7: transition point (requires ~200-300 km contiguous width)
 */
float biotic_pump_compute_L(float phi_f) {
    const float L_min = 6.0e5f;   /* 600 km */
    const float L_max = 2.0e6f;   /* 2000 km */
    const float beta = 2.5f;

    /* Clamp phi_f to [0, 1] */
    if (phi_f < 0.0f) phi_f = 0.0f;
    if (phi_f > 1.0f) phi_f = 1.0f;

    /* Power-law scaling */
    float phi_pow = powf(phi_f, beta);
    return L_min + (L_max - L_min) * phi_pow;
}

/**
 * Compute aerodynamic mixing enhancement phi(LAI, H_c).
 *
 * Higher LAI and taller canopies enhance turbulent mixing and transpiration
 * capacity ([4.1]). Empirical form:
 *
 *   phi(LAI, H_c) = (LAI / LAI_ref) * sqrt(H_c / H_c_ref)
 *
 * Reference values:
 *   LAI_ref = 5.0 (productive forest)
 *   H_c_ref = 30.0 m (typical canopy height)
 *
 * The sqrt(H_c) dependence comes from logarithmic wind profile in the
 * surface layer (roughness ~ H_c).
 */
float biotic_pump_compute_phi_aero(float LAI, float H_c) {
    const float LAI_ref = 5.0f;
    const float H_c_ref = 30.0f;

    /* Avoid division by zero */
    if (LAI < 0.1f) LAI = 0.1f;
    if (H_c < 1.0f) H_c = 1.0f;

    return (LAI / LAI_ref) * sqrtf(H_c / H_c_ref);
}

/* ========================================================================
 * CORE SOLVER
 * ======================================================================== */

/**
 * Main biotic pump momentum solver.
 *
 * Implements the coupled vegetation-atmosphere dynamics described in the
 * Edison specification. For each grid cell:
 *
 *   1. Compute vapor partial pressure p_v from ET, LAI, H_c, T
 *   2. Diagnose pressure gradient ∂p/∂x from p_v and forest continuity
 *   3. Advance wind momentum with PGF, Coriolis, and drag
 *
 * The drag term uses Grok's optimized sqrt-free semi-implicit kernel.
 */
void biotic_pump_step(
    const VegetationState* veg,
    const BioticPumpParams* params,
    size_t grid_size,
    float dt,
    float* u_wind,
    float* v_wind,
    float* pressure_gradient
) {
    if (!veg || !params || !u_wind || !v_wind || !pressure_gradient || grid_size == 0) {
        return;
    }

    /* Pre-compute common factors */
    const float dt_rho = dt / params->rho;
    const float rho_f = params->rho * params->f;
    const float inv_h_c = 1.0f / params->h_c;
    const float inv_dx = 1.0f / params->dx;
    const float inv_2dx = 0.5f / params->dx;

    /* Convert ET from mm/day to kg/m²/s for physical units
     * 1 mm/day = 1 kg/m²/day = (1/86400) kg/m²/s */
    const float ET_conversion = 1.0f / 86400.0f;

    /* ====================================================================
     * STEP 1: Compute vapor partial pressure field p_v for all cells
     * ==================================================================== */

    /* Temporary array for p_v field */
    float p_v_field[grid_size];

    for (size_t i = 0; i < grid_size; i++) {
        /* ----------------------------------------------------------------
         * STEP 1: Compute vapor partial pressure p_v [Eq 11]
         * ----------------------------------------------------------------
         *
         * p_v = r_T * e_s(T) * [RH_0 + k_E * E * phi(LAI, H_c)]
         *
         * Components:
         *   - e_s(T): saturation vapor pressure (from LUT)
         *   - RH_0: background relative humidity
         *   - k_E * E * phi: enhancement from ET and canopy properties
         */

        float e_s = biotic_pump_saturation_vapor_pressure(veg->Temp[i]);
        float phi_aero = biotic_pump_compute_phi_aero(veg->LAI[i], veg->H_c[i]);

        /* ET enhancement term (convert ET to physical units) */
        float ET_si = veg->ET[i] * ET_conversion;
        float ET_enhancement = params->k_E * ET_si * phi_aero;

        /* Total relative humidity (background + ET contribution) */
        float RH_total = params->RH_0 + ET_enhancement;

        /* Vapor partial pressure [Pa] */
        p_v_field[i] = params->r_T * e_s * RH_total;
    }

    /* ====================================================================
     * STEP 2: Compute spatial pressure gradient from p_v field
     * ==================================================================== */

    for (size_t i = 0; i < grid_size; i++) {
        /* ----------------------------------------------------------------
         * Finite-difference pressure gradient scaled by circulation params
         * ----------------------------------------------------------------
         *
         * The biotic pump pressure drop is proportional to the gradient
         * of vapor pressure (which drives condensation):
         *
         *   ∂p/∂x ≈ (h_γ / L(phi_f)) * (∂p_v/∂x) / h_c
         *
         * We use finite differences to compute ∂p_v/∂x.
         */

        float L = biotic_pump_compute_L(veg->phi_f[i]);
        float scale_factor = (params->h_gamma / L) * inv_h_c;

        float dp_v_dx;
        if (i == 0) {
            /* Forward difference at ocean boundary */
            dp_v_dx = (p_v_field[i + 1] - p_v_field[i]) * inv_dx;
        } else if (i == grid_size - 1) {
            /* Backward difference at inland boundary */
            dp_v_dx = (p_v_field[i] - p_v_field[i - 1]) * inv_dx;
        } else {
            /* Centered difference (interior) */
            dp_v_dx = (p_v_field[i + 1] - p_v_field[i - 1]) * inv_2dx;
        }

        /* Pressure gradient [Pa/m]
         *
         * SIGN CONVENTION: Higher vapor pressure → more condensation → LOWER total pressure
         * Therefore, dp/dx = -scale_factor * dp_v/dx
         *
         * This means if ET (and p_v) increase inland, pressure decreases inland,
         * creating a favorable gradient for ocean-to-land flow.
         */
        float dPdx = -scale_factor * dp_v_dx;

        /* Store for diagnostics */
        pressure_gradient[i] = dPdx;

        /* ----------------------------------------------------------------
         * STEP 3: Momentum update with Coriolis [Eq 12]
         * ----------------------------------------------------------------
         *
         * Explicit Euler for PGF and Coriolis:
         *   u_t = u + dt * (-dP/dx / rho + f * v)
         *   v_t = v + dt * (-f * u)
         */

        float rho_f_v = rho_f * v_wind[i];
        float rho_f_u = rho_f * u_wind[i];

        float u_t = u_wind[i] + dt_rho * (-dPdx + rho_f_v);
        float v_t = v_wind[i] + dt_rho * (-rho_f_u);

        /* ----------------------------------------------------------------
         * STEP 4: Semi-implicit drag (Grok optimization)
         * ----------------------------------------------------------------
         *
         * Grok's sqrt-free approximation:
         *   |u| ≈ max(|u|, |v|) + 0.5 * min(|u|, |v|)
         *
         * Error < 12% vs sqrt(u² + v²), but ~3× faster.
         *
         * Semi-implicit drag update:
         *   u_new = u_t / (1 + dt * c_d * |u_t|)
         *   v_new = v_t / (1 + dt * c_d * |v_t|)
         *
         * This is unconditionally stable (no CFL restriction).
         */

#if defined(__WASM_SIMD128__)
        /* ---- WASM SIMD PLACEHOLDER ----
         * Future optimization: vectorize 4 cells at a time
         *
         * v128_t u_vec = wasm_v128_load(&u_t);
         * v128_t v_vec = wasm_v128_load(&v_t);
         * v128_t abs_u = wasm_f32x4_abs(u_vec);
         * v128_t abs_v = wasm_f32x4_abs(v_vec);
         * v128_t max_val = wasm_f32x4_max(abs_u, abs_v);
         * v128_t min_val = wasm_f32x4_min(abs_u, abs_v);
         * v128_t approx_spd = wasm_f32x4_add(max_val, wasm_f32x4_mul(min_val, half));
         * ... (drag update)
         */
#endif

        /* Scalar version (current) */
        float abs_u = fabsf(u_t);
        float abs_v = fabsf(v_t);
        float approx_spd = fmaxf(abs_u, abs_v) + 0.5f * fminf(abs_u, abs_v);

        /* Add small floor to prevent division issues in calm conditions */
        approx_spd = fmaxf(approx_spd, 0.01f);

        /* Semi-implicit drag coefficient */
        float drag_factor = 1.0f + dt * params->c_d * approx_spd;

        /* Update winds */
        u_wind[i] = u_t / drag_factor;
        v_wind[i] = v_t / drag_factor;

    } /* End main loop */
}

/* ========================================================================
 * DIAGNOSTIC FUNCTIONS
 * ======================================================================== */

/**
 * Compute atmospheric moisture convergence for budget closure.
 *
 * Finite-difference approximation of:
 *   C = -d(u·W)/dx
 *
 * where W is column-integrated water vapor [kg/m²].
 *
 * This allows verification of the moisture balance ([6.1]):
 *   dW/dt = C + E - P
 *
 * Boundary conditions:
 *   - i=0 (ocean): forward difference
 *   - i=N-1 (inland): backward difference
 *   - interior: centered difference
 */
void biotic_pump_moisture_convergence(
    const float* u_wind,
    const float* W,
    float dx,
    size_t grid_size,
    float* convergence
) {
    if (!u_wind || !W || !convergence || grid_size < 2) {
        return;
    }

    const float inv_dx = 1.0f / dx;
    const float inv_2dx = 0.5f / dx;

    for (size_t i = 0; i < grid_size; i++) {
        float flux_i = u_wind[i] * W[i];

        if (i == 0) {
            /* Forward difference at ocean boundary */
            float flux_ip1 = u_wind[i + 1] * W[i + 1];
            convergence[i] = -(flux_ip1 - flux_i) * inv_dx;
        } else if (i == grid_size - 1) {
            /* Backward difference at inland boundary */
            float flux_im1 = u_wind[i - 1] * W[i - 1];
            convergence[i] = -(flux_i - flux_im1) * inv_dx;
        } else {
            /* Centered difference (interior) */
            float flux_ip1 = u_wind[i + 1] * W[i + 1];
            float flux_im1 = u_wind[i - 1] * W[i - 1];
            convergence[i] = -(flux_ip1 - flux_im1) * inv_2dx;
        }
    }
}
