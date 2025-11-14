/*
 * atmosphere_biotic.h - Biotic Pump Atmospheric Solver
 *
 * Implements condensation-induced atmospheric dynamics (CIAD) linking forest
 * evapotranspiration (ET) to horizontal pressure gradients and winds.
 *
 * Scientific Foundation:
 *   This solver translates the Biotic Pump theory into computational form,
 *   implementing the mechanistic coupling between vegetation transpiration,
 *   atmospheric moisture convergence, and momentum dynamics.
 *
 * Core Principle (Makarieva & Gorshkov 2010, [1.1]):
 *   Condensation of water vapor creates local pressure drops. When forests
 *   maintain high ET, the resulting condensation generates horizontal pressure
 *   gradients that drive ocean-to-land moisture transport, sustaining
 *   continental precipitation far from coasts.
 *
 * Key Equations (from Edison Research Specification):
 *
 *   [Eq 10] Pressure gradient from ET and forest continuity:
 *       ∂p/∂x = (h_γ/L) · (1/h_c) · p_v(E, LAI, H_c)
 *
 *   [Eq 11] Vapor partial pressure:
 *       p_v ≈ r_T · e_s(T) · [RH_0 + k_E · E · φ(LAI, H_c)]
 *
 *   [Eq 13] Wind velocity (friction-dominated limit):
 *       u ≈ [(1/ρc_d) · (h_γ/L) · (p_v/h_c)]^(1/2)
 *
 *   [Eq 12] Full momentum balance with Coriolis and drag:
 *       c_d·u|u| + (f²/c_d)·(u/|u|) ≈ (1/ρ) · (h_γ/L) · (p_v/h_c)
 *
 * Parameter Ranges (from [2.1], [3.1], [4.1], [5.1]):
 *   h_γ     : 800-2500 m      (circulation depth)
 *   h_c     : 1500-2500 m     (vapor scale height)
 *   L       : 10⁵-2×10⁶ m    (inland length scale; forests ~10⁶ m)
 *   c_d     : 10⁻⁴-5×10⁻³    (canopy drag coefficient)
 *   LAI     : 3-7             (leaf area index)
 *   H_c     : 15-50 m         (canopy height)
 *   E       : 1-6 mm/day      (evapotranspiration)
 *   α       : 0.6-1.0         (condensation efficiency)
 *   φ_f     : 0-1             (forest continuity)
 *
 * Threshold Behavior ([2.1], [3.1]):
 *   - Non-forested: P(x) decays exponentially, e-folding ~600 km
 *   - Forested (φ_f > φ_crit): P(x) sustained over >10³ km
 *   - Critical continuity: φ_f ≳ 0.6, requiring contiguous forest
 *     widths of ~200-300 km to maintain L ≳ 10⁶ m
 *
 * Implementation Notes:
 *   - Pure function design: stateless, thread-safe
 *   - Performance optimized: Clausius-Clapeyron lookup table (256 entries)
 *   - Drag kernel: sqrt-free semi-implicit scheme (Grok optimization)
 *   - SIMD-ready: structured for future vectorization
 *
 * Design Principles:
 *   - Stateless: All data passed explicitly
 *   - Side-effect-free: No global state
 *   - Thread-safe: Can be parallelized
 *   - Physically consistent: Mass-conserving, energy-conserving
 *
 * References:
 *   [1.1] Makarieva & Gorshkov (2010), Int. J. Water
 *   [1.2] Makarieva & Gorshkov (2010), condensation dynamics
 *   [2.1] Makarieva & Gorshkov (2007), Hydrol. Earth Syst. Sci.
 *   [3.1] Makarieva et al. (2009), Ecol. Complexity
 *   [4.1] Katul et al. (2012), Rev. Geophys.
 *   [5.1] Makarieva et al. (2022), Heliyon
 *   [6.1] Makarieva et al. (2023), Global Change Biol.
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (ATMv1)
 * License: MIT OR GPL-3.0
 */

#ifndef ATMOSPHERE_BIOTIC_H
#define ATMOSPHERE_BIOTIC_H

#include <stddef.h>

/* ========================================================================
 * PARAMETER STRUCTURES
 * ======================================================================== */

/**
 * Biotic pump physical parameters.
 *
 * These parameters control the strength of the condensation-induced
 * pressure gradient and the resulting atmospheric circulation.
 */
typedef struct {
    float h_gamma;      /* Effective circulation depth [m]
                         * Range: 800-2500 m ([1.2], [5.1])
                         * Physical meaning: vertical extent of moist inflow
                         * and condensation return flow */

    float h_c;          /* Water vapor scale height [m]
                         * Range: 1500-2500 m ([5.1], [5.2])
                         * Physical meaning: characteristic height over which
                         * vapor density decreases by factor e */

    float c_d;          /* Canopy drag coefficient [m⁻¹]
                         * Range: 1e-4 to 5e-3 ([4.1])
                         * Physical meaning: bulk drag over forest canopy
                         * (depth-averaged) */

    float f;            /* Coriolis parameter [s⁻¹]
                         * Typical: ~1e-4 (mid-latitudes)
                         * Physical meaning: f = 2Ω sin(lat), where Ω is
                         * Earth's rotation rate */

    float rho;          /* Air density [kg/m³]
                         * Typical: ~1.2 kg/m³ at sea level
                         * Physical meaning: mass per unit volume of air */

    float r_T;          /* Vapor mixing ratio coefficient [-]
                         * Typical: ~0.622 (ratio of molecular weights)
                         * Physical meaning: relates partial pressure to
                         * mixing ratio */

    float RH_0;         /* Base relative humidity [-]
                         * Range: 0.5-0.9
                         * Physical meaning: background atmospheric moisture */

    float k_E;          /* ET enhancement coefficient [day/mm]
                         * Typical: 0.01-0.1
                         * Physical meaning: how much ET increases vapor
                         * pressure above background ([1.2]) */

    float dx;           /* Grid spacing [m]
                         * Typical: 1e4-1e5 m (10-100 km)
                         * Physical meaning: horizontal resolution */

} BioticPumpParams;

/**
 * Vegetation state fields.
 *
 * These arrays describe the spatial distribution of forest properties
 * that drive the biotic pump mechanism.
 */
typedef struct {
    const float* ET;          /* Evapotranspiration field [mm/day]
                               * Range: 1-6 mm/day ([4.1])
                               * Physical meaning: rate of water vapor flux
                               * from surface to atmosphere */

    const float* LAI;         /* Leaf Area Index field [-]
                               * Range: 3-7 for productive forests ([4.1])
                               * Physical meaning: one-sided leaf area per
                               * unit ground area */

    const float* H_c;         /* Canopy height field [m]
                               * Range: 15-50 m ([4.1])
                               * Physical meaning: average vegetation height,
                               * influences aerodynamic roughness */

    const float* phi_f;       /* Forest continuity field [-]
                               * Range: 0-1 ([2.1])
                               * Physical meaning: fraction of contiguous
                               * forest along fetch (1 = continuous forest,
                               * 0 = non-forested) */

    const float* Temp;        /* Temperature field [K]
                               * Typical: 273-313 K (0-40°C)
                               * Physical meaning: air temperature, used for
                               * Clausius-Clapeyron relation e_s(T) */
} VegetationState;

/* ========================================================================
 * SOLVER FUNCTIONS
 * ======================================================================== */

/**
 * Initialize the Biotic Pump solver.
 *
 * Pre-computes lookup tables for temperature-dependent vapor pressure
 * (Clausius-Clapeyron relation) to avoid expensive exp() calls in the
 * main solver loop.
 *
 * This function must be called once before using biotic_pump_step().
 *
 * Performance: Grok optimization - 256-entry LUT reduces per-cell cost
 * from ~100ns to ~5ns for e_s(T) evaluation.
 */
void biotic_pump_init(void);

/**
 * Advance atmospheric state by one timestep using Biotic Pump dynamics.
 *
 * Implements the coupling between vegetation ET and atmospheric momentum:
 *   1. Compute vapor partial pressure p_v from ET, LAI, H_c, T [Eq 11]
 *   2. Compute pressure gradient ∂p/∂x from p_v and forest continuity [Eq 10]
 *   3. Update wind momentum with PGF and Coriolis terms
 *   4. Apply semi-implicit drag (Grok's sqrt-free kernel)
 *
 * Numerical Scheme:
 *   - Explicit Euler for pressure-gradient force and Coriolis
 *   - Semi-implicit for drag (unconditionally stable)
 *   - No CFL restriction from drag term
 *
 * Mass Conservation:
 *   Continuity relation uhγ = wL is implicit in the pressure-gradient
 *   formulation [Eq 14]. Total column moisture should be monitored
 *   externally for budget closure.
 *
 * Boundary Conditions:
 *   - Ocean boundary (i=0): prescribe u_wind based on ocean-land ET contrast
 *   - Inland boundary (i=N-1): free outflow or periodic (depends on domain)
 *
 * @param veg           Vegetation state (ET, LAI, H_c, phi_f, Temp)
 * @param params        Physical parameters (h_gamma, h_c, c_d, f, rho, ...)
 * @param grid_size     Number of grid points along transect
 * @param dt            Timestep [s]
 * @param u_wind        [IN/OUT] Horizontal (along-transect) wind [m/s]
 * @param v_wind        [IN/OUT] Meridional (cross-transect) wind [m/s]
 * @param pressure_gradient [OUT] Emergent pressure gradient [Pa/m]
 *
 * Thread Safety: Pure function, can be called concurrently with different
 *                state arrays.
 *
 * Performance: ~10 ns/cell on modern x86-64 (Grok projection with SIMD)
 */
void biotic_pump_step(
    const VegetationState* veg,
    const BioticPumpParams* params,
    size_t grid_size,
    float dt,
    float* u_wind,
    float* v_wind,
    float* pressure_gradient
);

/**
 * Compute inland length scale L from forest continuity phi_f.
 *
 * Empirical relation from [2.1], [3.1]:
 *   - Non-forested (phi_f → 0): L ~ 6e5 m (e-folding ~600 km)
 *   - Forested (phi_f → 1): L ~ 1-2e6 m (sustained over >1000 km)
 *
 * Functional form (power law):
 *   L(phi_f) = L_min + (L_max - L_min) * phi_f^beta
 *
 * where beta > 1 captures threshold behavior (rapid transition near
 * phi_f ~ 0.6-0.7).
 *
 * @param phi_f Forest continuity [0,1]
 * @return Inland length scale [m]
 */
float biotic_pump_compute_L(float phi_f);

/**
 * Compute aerodynamic mixing enhancement phi(LAI, H_c).
 *
 * Higher LAI and taller canopies increase turbulent mixing and
 * transpiration capacity ([4.1]).
 *
 * Functional form (empirical):
 *   phi(LAI, H_c) = (LAI / LAI_ref) * (H_c / H_c_ref)^0.5
 *
 * where LAI_ref = 5, H_c_ref = 30 m are reference forest values.
 *
 * @param LAI Leaf area index [-]
 * @param H_c Canopy height [m]
 * @return Aerodynamic enhancement factor [-]
 */
float biotic_pump_compute_phi_aero(float LAI, float H_c);

/* ========================================================================
 * DIAGNOSTIC FUNCTIONS
 * ======================================================================== */

/**
 * Compute saturation vapor pressure e_s(T) via Clausius-Clapeyron.
 *
 * Uses lookup table initialized by biotic_pump_init().
 *
 * Approximation (August-Roche-Magnus):
 *   e_s(T) = 611.2 * exp(17.67 * (T - 273.15) / (T - 29.65))  [Pa]
 *
 * Valid range: 243 K to 333 K (-30°C to +60°C)
 *
 * @param T_kelvin Temperature [K]
 * @return Saturation vapor pressure [Pa]
 */
float biotic_pump_saturation_vapor_pressure(float T_kelvin);

/**
 * Compute total atmospheric moisture convergence for budget closure.
 *
 * Convergence C is the net horizontal moisture flux into the column:
 *   C = ∫ ∇·(ρ_v u) dz ≈ -d(u W)/dx
 *
 * where W is column-integrated water vapor.
 *
 * This diagnostic helps verify the moisture balance:
 *   dW/dt = C + E - P
 *
 * as described in [6.1].
 *
 * @param u_wind Horizontal wind field [m/s]
 * @param W Column water vapor field [kg/m²]
 * @param dx Grid spacing [m]
 * @param grid_size Number of grid points
 * @param convergence [OUT] Moisture convergence field [kg/m²/s]
 */
void biotic_pump_moisture_convergence(
    const float* u_wind,
    const float* W,
    float dx,
    size_t grid_size,
    float* convergence
);

#endif /* ATMOSPHERE_BIOTIC_H */
