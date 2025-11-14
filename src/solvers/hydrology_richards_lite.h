/*
 * hydrology_richards_lite.h - Richards-Lite Hydrology Solver
 *
 * Implements generalized Richards equation with microscale earthwork interventions
 * (swales, mulches, check dams) and explicit surface–subsurface coupling.
 *
 * Scientific Foundation:
 *   This solver implements Edison's "Generalized Richards Equation" framework
 *   with Grok's high-performance "Richards-Lite" numerical scheme, combining
 *   scientific accuracy with real-time performance requirements.
 *
 * Core Principle (Weill et al. 2009, [1.1]):
 *   A unified Richards-type PDE with a thin porous "runoff layer" at the surface
 *   enforces pressure/flux continuity and represents overland flow as a Darcy-like
 *   diffusive wave. This single equation handles both infiltration-excess (Hortonian)
 *   and saturation-excess (Dunne) runoff without explicit switching.
 *
 * Key Equations (from Edison Research Specification):
 *
 *   [Generalized Richards] Surface–subsurface flow with intervention:
 *       ∂θ/∂t = ∇·(K_eff(θ,I,ζ) ∇(ψ+z)) + S_I(x,y,t)
 *
 *   [Effective Conductivity] Intervention modulation:
 *       K_eff(θ,I,ζ) = K_mat(θ) · M_I(θ,s;slope,w,d) · C(ζ)
 *
 *   [Fill-and-Spill Connectivity]:
 *       C(ζ) = 1/(1 + exp[-a_c(ζ - ζ_c)])
 *
 *   [Runoff Layer] Darcy-film overland flow:
 *       q_s = -K_r(θ) ∇(η_s),    η_s = h_s + z
 *
 * Intervention Effects (Edison Empirical Data):
 *   - Gravel mulch: 93% runoff reduction, 6× infiltration depth, 4× evaporation suppression
 *   - Swales: 50-70% annual volume reduction for suitable designs
 *   - Microtopography: Fill-and-spill threshold ζ_c = 2-20 mm
 *
 * Numerical Scheme (Grok's "Richards-Lite v2"):
 *   1. Vertical implicit pass: unconditionally stable 1D solve (gravity + infiltration)
 *   2. Horizontal explicit pass: conditional surface flow (activated by connectivity)
 *   3. Lookup tables: van Genuchten θ(ψ) and K(θ) pre-computed (256 entries)
 *   4. Performance target: < 200 ns/cell (target < 100 ns/cell)
 *
 * FUTURE COUPLING (REGv1 Integration):
 *   This solver is designed for tight coupling with the REGv1 "Regeneration Cascade"
 *   solver, which implements macro-scale vegetation–SOM–moisture feedbacks.
 *
 *   Coupling mechanism (implemented via Cell state variables):
 *   - REGv1 modifies: porosity_eff, K_tensor (read by HYD every step)
 *   - Hydrological bonus: +1% SOM → +5mm water holding + +15% K_zz
 *   - See docs/science/macroscale_regeneration.md for details
 *
 * Implementation Notes:
 *   - Pure function design: stateless, thread-safe
 *   - Performance optimized: 256-entry lookup tables for θ(ψ), K(θ)
 *   - Operator splitting: implicit vertical + explicit horizontal
 *   - SIMD-ready: structured for future vectorization
 *
 * Design Principles:
 *   - Stateless: All data passed explicitly via Cell structs
 *   - Side-effect-free: No global state
 *   - Thread-safe: Can be parallelized
 *   - Physically consistent: Mass-conserving, energy-conserving
 *
 * References:
 *   [1.1] Weill, Mouche, Patin (2009), J. Hydrology - Generalized Richards equation
 *   [2.1] Frei, Lischeid, Fleckenstein (2010), Adv. Water Resour. - Microtopography
 *   [3.1] Garcia-Serrana, Gulliver, Nieber (2016) - Minnesota swale calculator
 *   [4.1] Li (2003), Catena - Gravel mulch effects (Loess Plateau)
 *   See docs/science/microscale_hydrology.md for complete references
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (HYD-RLv1)
 * License: MIT OR GPL-3.0
 */

#ifndef HYDROLOGY_RICHARDS_LITE_H
#define HYDROLOGY_RICHARDS_LITE_H

#include <stddef.h>

/* ========================================================================
 * CORE DATA STRUCTURES
 * ======================================================================== */

/**
 * Cell - Spatially-distributed hydrological state and parameters.
 *
 * This structure represents a single computational cell (grid element) in
 * the hydrology model. It contains both:
 *   - Hydrological state (θ, ψ, h_surface)
 *   - Soil/intervention parameters (K_s, porosity, multipliers)
 *   - Regeneration state (vegetation_cover, SOM_percent) [REGv1 coupling]
 *
 * CRITICAL DESIGN NOTE (REGv1 Integration):
 * The fields vegetation_cover, SOM_percent, porosity_eff, and K_tensor
 * are STATE VARIABLES that will be modified by the future REGv1 solver.
 * The HYD-RLv1 solver MUST read these values (not use hardcoded parameters)
 * to enable the "hydrological bonus" feedback mechanism:
 *   +1% SOM → +5mm water holding (via porosity_eff)
 *   +1% SOM → +15% vertical conductivity (via K_tensor[ZZ])
 *
 * Memory Layout:
 *   - Designed for cache-friendly access patterns
 *   - SOA (struct-of-arrays) conversion possible for SIMD
 */
typedef struct {
    /* ---- Hydrological State (fast-changing) ---- */
    float theta;            /* Volumetric water content [m³/m³]
                             * Range: [theta_r, theta_s] typically 0.05-0.45
                             * Physical meaning: fraction of pore space filled */

    float psi;              /* Matric head (pressure head) [m]
                             * Range: [-10^5, 0] m (dry to saturated)
                             * Physical meaning: capillary suction, negative when unsaturated */

    float h_surface;        /* Surface water depth [m]
                             * Range: [0, ~0.1] m (ponding depth)
                             * Physical meaning: depth of water above soil surface */

    float zeta;             /* Depression storage [m]
                             * Range: [0, zeta_max] typically 0-0.02 m
                             * Physical meaning: water stored in microtopographic depressions */

    /* ---- Soil Hydraulic Parameters (slow-changing or static) ---- */
    float K_s;              /* Saturated hydraulic conductivity [m/s]
                             * Range: [10^-7, 10^-4] m/s (clay to sand)
                             * Physical meaning: water flow rate when soil is saturated */

    float alpha_vG;         /* van Genuchten air entry parameter [1/m]
                             * Range: [0.5, 10] m^-1 typical
                             * Physical meaning: inverse of air-entry pressure scale */

    float n_vG;             /* van Genuchten pore size distribution [-]
                             * Range: [1.1, 3.0] typical
                             * Physical meaning: controls curve shape, m = 1-1/n */

    float theta_s;          /* Saturated water content [m³/m³]
                             * Range: [0.3, 0.6] typical
                             * Physical meaning: porosity (total pore space) */

    float theta_r;          /* Residual water content [m³/m³]
                             * Range: [0.01, 0.15] typical
                             * Physical meaning: water remaining at very dry conditions */

    /* ---- Intervention Multipliers (microscale earthworks) ---- */
    float M_K_zz;           /* Vertical K multiplier [-]
                             * Range: [0.5, 6.0]
                             * Mulch: 2-6 (deeper infiltration, Edison [4.1])
                             * Swale: 1-3 (depends on geometry, Edison [3.1])
                             * Physical meaning: how intervention enhances infiltration */

    float M_K_xx;           /* Horizontal K multiplier (along-contour) [-]
                             * Range: [0.5, 3.0]
                             * Physical meaning: channeling effect of swales/berms */

    float kappa_evap;       /* Evaporation suppression factor [-]
                             * Range: [0.25, 1.0]
                             * Mulch: 0.25-0.5 (4× suppression, Edison [4.1])
                             * Bare: 1.0
                             * Physical meaning: shading/cooling effect */

    float Delta_zeta;       /* Added depression storage [m]
                             * Range: [0, 0.002] m (0-2 mm)
                             * Mulch interception: 0.5-0.8 mm per event (Edison [4.1])
                             * Check dams: larger values upstream
                             * Physical meaning: extra water storage capacity */

    /* ---- Microtopography Parameters ---- */
    float zeta_c;           /* Fill-and-spill threshold [m]
                             * Range: [0.002, 0.020] m (2-20 mm)
                             * Physical meaning: depression must fill to this depth
                             * before surface connectivity activates (Frei [2.1]) */

    float a_c;              /* Connectivity steepness [1/m]
                             * Range: [0.2, 2.0] m^-1
                             * Physical meaning: sharpness of fill-and-spill transition */

    /* ---- REGv1 Coupling: Regeneration State (modified by future solver) ---- */
    float vegetation_cover; /* Vegetation fractional cover [-]
                             * Range: [0, 1]
                             * Modified by: REGv1 regeneration_cascade_step()
                             * Units: fraction (0 = bare, 1 = full cover)
                             * Physical meaning: living biomass coverage */

    float SOM_percent;      /* Soil organic matter content [%]
                             * Range: [0.01, 10.0] % typical
                             * Modified by: REGv1 regeneration_cascade_step()
                             * Units: percent by mass (top layer 0-30 cm)
                             * Physical meaning: carbon storage + water-holding capacity */

    /* ---- REGv1 Coupling: Effective Parameters (computed by REGv1, read by HYD) ---- */
    float porosity_eff;     /* Effective porosity [m³/m³]
                             * Range: [theta_s, theta_s + 0.05]
                             * Modified by: REGv1 (porosity_eff += 0.005 * dSOM)
                             * Hydrological bonus: +1% SOM → +5mm water holding
                             * Physical meaning: total pore space including SOM effect */

    float K_tensor[9];      /* Anisotropic hydraulic conductivity tensor [m/s]
                             * Layout: [Kxx, Kxy, Kxz, Kyx, Kyy, Kyz, Kzx, Kzy, Kzz]
                             * Modified by: REGv1 (K_tensor[8] *= 1.15^dSOM)
                             * Hydrological bonus: +1% SOM → +15% K_zz
                             * Physical meaning: direction-dependent flow capacity
                             * Note: For isotropic case, off-diagonals = 0 */

    /* ---- Grid Geometry ---- */
    float z;                /* Elevation above datum [m]
                             * Physical meaning: gravitational potential */

    float dz;               /* Vertical cell size [m]
                             * Typical: 0.05-0.20 m (5-20 cm layers) */

    float dx;               /* Horizontal cell size [m]
                             * Typical: 1-100 m (plot to catchment scale) */

} Cell;

/**
 * RichardsLiteParams - Global solver parameters and physical constants.
 *
 * These parameters control the numerical scheme and physical processes.
 * They are typically constant across the entire simulation domain.
 */
typedef struct {
    /* ---- Runoff Layer Parameters (Weill et al. 2009 [1.1]) ---- */
    float K_r;              /* Runoff layer conductivity [m/s]
                             * Range: [10^-6, 10^-3] m/s
                             * Physical meaning: overland flow as Darcy film
                             * Typical: 10^-4 m/s for sheet flow */

    float phi_r;            /* Runoff layer porosity [-]
                             * Range: [0.3, 0.9]
                             * Physical meaning: equivalent porous medium
                             * Typical: 0.5 (conceptual parameter) */

    float l_r;              /* Runoff layer thickness [m]
                             * Range: [0.001, 0.010] m (1-10 mm)
                             * Physical meaning: thin film approximation
                             * Typical: 0.005 m (5 mm) */

    /* ---- Transmissivity Feedback (Frei et al. 2010 [2.1]) ---- */
    float b_T;              /* Transmissivity feedback coefficient [1/m]
                             * Range: [0.5, 3.0] m^-1
                             * Physical meaning: T(η) = T_0 exp(b_T (η-η_0))
                             * Controls exponential K increase as water table rises */

    /* ---- Evaporation Parameters ---- */
    float E_bare_ref;       /* Reference bare-soil evaporation [m/s]
                             * Range: [10^-8, 10^-6] m/s (0.8-8.6 mm/day)
                             * Physical meaning: potential evaporation rate
                             * Climate-dependent; calibrate to local PET */

    /* ---- Numerical Scheme Control ---- */
    float dt_max;           /* Maximum timestep [s]
                             * Range: [60, 3600] s (1 min to 1 hour)
                             * Physical meaning: stability limit for explicit pass */

    float CFL_factor;       /* CFL safety factor for explicit horizontal pass [-]
                             * Range: [0.3, 0.9]
                             * Physical meaning: fraction of stability limit
                             * Typical: 0.5 (50% of CFL limit) */

    float picard_tol;       /* Picard iteration convergence tolerance [-]
                             * Range: [10^-6, 10^-3]
                             * Physical meaning: relative change in θ
                             * Typical: 10^-4 */

    int picard_max_iter;    /* Maximum Picard iterations [-]
                             * Range: [5, 50]
                             * Physical meaning: nonlinearity solver limit
                             * Typical: 20 */

} RichardsLiteParams;

/**
 * InterventionType - Enumeration of earthwork intervention types.
 *
 * Used for setting Cell multipliers and diagnostics.
 */
typedef enum {
    INTERVENTION_NONE = 0,          /* No intervention (bare or native soil) */
    INTERVENTION_MULCH_GRAVEL = 1,  /* Gravel/sand mulch (Edison [4.1]) */
    INTERVENTION_SWALE = 2,          /* Grassed roadside swale (Edison [3.1]) */
    INTERVENTION_BERM = 3,           /* Contour berm / check dam */
    INTERVENTION_BIOCRUST = 4        /* Biological soil crust */
} InterventionType;

/* ========================================================================
 * SOLVER FUNCTIONS
 * ======================================================================== */

/**
 * Initialize the Richards-Lite solver.
 *
 * Pre-computes lookup tables for:
 *   - θ(ψ) van Genuchten retention curve (256 entries)
 *   - K(θ) Mualem conductivity curve (256 entries)
 *   - dθ/dψ specific moisture capacity (256 entries, for Picard iteration)
 *
 * This function must be called once before using richards_lite_step().
 *
 * Performance: Grok optimization - 256-entry LUTs reduce per-cell cost
 * from ~200ns to ~15ns for θ(ψ) and K(θ) evaluations (similar to ATMv1's
 * 20× speedup for Clausius-Clapeyron).
 */
void richards_lite_init(void);

/**
 * Advance hydrological state by one timestep using Richards-Lite dynamics.
 *
 * Implements Grok's hybrid "Richards-Lite v2" scheme:
 *   1. Update surface depression storage ζ and connectivity C(ζ)
 *   2. Vertical implicit pass: solve 1D Richards in z-direction (tridiagonal)
 *   3. Horizontal explicit pass: surface flow if C(ζ) > threshold (CFL-limited)
 *   4. Apply evaporation sink E_eff = kappa_evap * E_bare
 *
 * Numerical Scheme:
 *   - Vertical: implicit Euler (unconditionally stable, no CFL restriction)
 *   - Horizontal: explicit diffusion (CFL-limited, only when ζ > ζ_c)
 *   - Thomas algorithm for tridiagonal solves (O(N) per column)
 *   - Picard iteration for nonlinearity (K(θ) depends on θ)
 *
 * Mass Conservation:
 *   Total water balance tracked via diagnostics:
 *     dW/dt = P - E - R - D + S_I
 *   where W=total water, P=rainfall, E=evaporation, R=runoff, D=drainage,
 *   S_I=intervention sources (check dam inflow, etc.)
 *
 * Boundary Conditions:
 *   - Top surface (z=0): rainfall flux P(t) [m/s] or Dirichlet h=0
 *   - Bottom (z=z_max): free drainage ∂ψ/∂z = -1 or prescribed water table
 *   - Lateral (x boundaries): periodic or free outflow
 *
 * REGv1 Coupling:
 *   This solver READS the following Cell state variables that are modified
 *   by the REGv1 regeneration_cascade_step():
 *     - porosity_eff: water-holding capacity (+5mm per 1% SOM)
 *     - K_tensor[8]: vertical conductivity (+15% per 1% SOM, compounding)
 *
 *   The coupling is one-way per timestep: REGv1 runs every ~128 hydrology
 *   steps, modifies Cell state, then HYD reads the updated values.
 *
 * @param cells         [IN/OUT] Array of Cell structs (grid)
 * @param params        [IN] Solver parameters (global constants)
 * @param nx            Number of cells in x-direction
 * @param ny            Number of cells in y-direction
 * @param nz            Number of vertical layers
 * @param dt            Timestep [s]
 * @param rainfall      Rainfall rate [m/s] (spatially uniform for now)
 * @param diagnostics   [OUT] Optional diagnostics struct (can be NULL)
 *
 * Thread Safety: Pure function, can be called concurrently with different
 *                Cell arrays.
 *
 * Performance: Target ~15-20 ns/cell on modern x86-64 with optimized LUTs
 *              (comparable to ATMv1's ~10 ns/cell for simpler physics)
 */
void richards_lite_step(
    Cell* cells,
    const RichardsLiteParams* params,
    size_t nx,
    size_t ny,
    size_t nz,
    float dt,
    float rainfall,
    void* diagnostics  /* Reserved for future mass balance tracking */
);

/**
 * Apply intervention multipliers to a Cell.
 *
 * Sets M_K_zz, kappa_evap, Delta_zeta based on intervention type and
 * empirical data from Edison research (Loess Plateau field studies).
 *
 * Empirical Anchors (from docs/science/microscale_hydrology.md):
 *   - Gravel mulch: M_K_zz=2-6, kappa_evap=0.25-0.5, Delta_zeta=0.5-0.8mm
 *   - Swale: M_K_zz=1-3 (depends on slope, width, wetted fraction)
 *   - Check dam: Delta_zeta increases upstream, K_r decreases downstream
 *
 * @param cell          [IN/OUT] Cell to modify
 * @param intervention  Intervention type
 * @param intensity     Intervention strength [0,1] (e.g., mulch cover fraction)
 */
void richards_lite_apply_intervention(
    Cell* cell,
    InterventionType intervention,
    float intensity
);

/* ========================================================================
 * DIAGNOSTIC FUNCTIONS
 * ======================================================================== */

/**
 * Compute fill-and-spill connectivity function C(ζ).
 *
 * Implements thresholded microtopography response (Frei et al. 2010 [2.1]):
 *   C(ζ) = 1 / (1 + exp[-a_c(ζ - ζ_c)])
 *
 * Physical meaning:
 *   - ζ < ζ_c: subsurface-dominated (C → 0, no surface flow)
 *   - ζ > ζ_c: surface-dominated (C → 1, active overland flow)
 *   - Hysteretic behavior: rising vs falling water table
 *
 * @param zeta  Depression storage [m]
 * @param zeta_c Threshold [m]
 * @param a_c   Steepness [1/m]
 * @return Connectivity factor [0,1]
 */
float richards_lite_connectivity(float zeta, float zeta_c, float a_c);

/**
 * Compute total water storage in a Cell [m].
 *
 * Total water = vadose zone + surface ponding + depression storage:
 *   W = θ * dz + h_surface + ζ
 *
 * Used for mass balance diagnostics.
 *
 * @param cell Cell to query
 * @return Total water column [m]
 */
float richards_lite_total_water(const Cell* cell);

/**
 * Classify runoff mechanism for a Cell.
 *
 * Determines whether runoff (if present) is:
 *   - Infiltration-excess (Hortonian): rainfall > infiltration capacity
 *   - Saturation-excess (Dunne): water table at surface
 *   - None: all rainfall infiltrated
 *
 * This diagnostic verifies that the unified PDE correctly reproduces both
 * mechanisms without explicit switching (Validation Test #4).
 *
 * @param cell Cell to classify
 * @param rainfall Current rainfall rate [m/s]
 * @return 0=none, 1=Hortonian, 2=Dunne
 */
int richards_lite_runoff_mechanism(const Cell* cell, float rainfall);

#endif /* HYDROLOGY_RICHARDS_LITE_H */
