/*
 * regeneration_microbial.h - Microbial Priming & Condenser Landscapes (REGv2)
 *
 * Implements the microscale biological and atmospheric-interface dynamics that
 * drive explosive, nonlinear regeneration through fungal priming, soil
 * aggregation, condensation physics, hydraulic lift, and biological precipitation.
 *
 * Scientific Foundation:
 *   This solver implements Edison's "Microbial Detonation Charges" - the biological
 *   primitives for fungal-bacterial dynamics, AMF/EPS-driven aggregation, fog/dew
 *   physics, hydraulic redistribution, and bioprecipitation feedbacks.
 *
 * Core Principle:
 *   The simulated land has an innate desire to heal. Once critical biological
 *   triggers activate (high F:B ratio, functional mycorrhizae, vegetation cover),
 *   multiple positive feedbacks amplify:
 *     - Fungal priming → explosive SOM accumulation (up to 8× multiplier)
 *     - Aggregation → nonlinear infiltration jumps (threshold at Φ_agg ≈ 0.5)
 *     - Condensers → dependable nonrainfall water (0.1-5 mm/d)
 *     - Hydraulic lift → nightly topsoil rehydration (0.1-1.3 mm/night)
 *     - Biological INPs → precipitation probability boost (+5-15%)
 *
 * Key Equations (from Edison Research Specification):
 *
 *   [SOM with Fungal Priming]
 *       dSOM/dt = P_micro(C_labile, θ, T, N_fix, Φ_agg, F:B) - D_resp(T, θ, O2)
 *       P_micro = P_max * P_Fb(F:B) * [Monod-Arrhenius terms]
 *       P_Fb: 8-entry lookup table with hard anchors (1.0→2.5x at F:B=1.0)
 *
 *   [Aggregation-Conductivity Linkage]
 *       K(θ) = K0(θ) * [1 + m_agg*Φ_agg*S(Φ_agg)] * [1 + α_myco*Φ_hyphae] * R(θ)
 *       S(Φ_agg): sigmoid threshold at Φ_c ≈ 0.5 (connectivity jump)
 *       R(θ): moisture-dependent repellency gate
 *
 *   [Condensation Flux]
 *       C_cond = ρ_w * Λ * (RH - RH_sat)⁺ * f_r * M_rock * M_veg + condenser_bonus
 *       M_rock = 1 + β_rock * ΔT_night (rock-mulch thermal mass)
 *
 *   [Hydraulic Lift (Night-Only)]
 *       Q_lift = k_root * (θ_deep - θ_shallow) * H * χ_night
 *       Bounds: 0.1-1.3 mm H2O night⁻¹
 *
 *   [Biological Rain Bonus]
 *       Δp_bio = [5-15%] when V > 0.6 AND F:B ≥ 2.0 (fungal-dominated)
 *
 * Performance Constraints (Grok Verified):
 *   - Computation budget: <50 cycles/cell for all mechanisms combined
 *   - All pseudocode confirmed efficient by Grok
 *   - Integration with REGv1 and HYD-RLv1: minimal overhead
 *
 * Coupling to Existing Solvers:
 *   [REGv1 (regeneration_cascade.c)]:
 *     - Replace simple dSOM/dt with P_micro() - D_resp()
 *     - Infuses SOM dynamics with microbial physics
 *
 *   [HYD-RLv1 (hydrology_richards_lite.c)]:
 *     - Apply K_unsat() to modify effective conductivity
 *     - Add C_cond() as surface water source (condensation/dew)
 *     - Add Q_lift() as nighttime water redistribution
 *
 * Implementation Notes:
 *   - Pure function design: stateless, thread-safe
 *   - Fixed-point friendly: all operations <50 cycles/cell
 *   - Physically consistent: Empirically grounded parameter ranges
 *   - Threshold-driven: Captures explosive state flips
 *
 * Design Principles:
 *   - Faithful to Edison's pseudocode: verbatim implementation
 *   - Grok-approved efficiency: all operations within budget
 *   - Empirically anchored: every parameter range has citations
 *   - Explosive recovery: nonlinear thresholds enable rapid state transitions
 *
 * References:
 *   See config/parameters/REGv2_Microbial.json for complete parameter
 *   tables, ranges, confidence levels, and literature citations [1.1]-[11.1].
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (REGv2)
 * License: MIT OR GPL-3.0
 */

#ifndef REGENERATION_MICROBIAL_H
#define REGENERATION_MICROBIAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "hydrology_richards_lite.h"  /* For Cell struct definition */

/* ========================================================================
 * CORE DATA STRUCTURES
 * ======================================================================== */

/**
 * REGv2_SOM_Params - Parameters for microbial-mediated SOM accumulation.
 *
 * Controls the production (P_micro) and respiration (D_resp) terms in the
 * fungal-priming-enhanced SOM balance equation.
 */
typedef struct {
    /* Production term (P_micro) */
    float P_max;           /* Maximum microbial SOM production rate [g C m⁻² d⁻¹]
                            * Range: [0.1, 5.0], default: 2.5 */

    float K_C;             /* Half-saturation for labile carbon [g C m⁻²]
                            * Range: [5.0, 50.0], default: 20.0 */

    float K_theta;         /* Half-saturation for soil moisture [m³ m⁻³]
                            * Range: [0.05, 0.20], default: 0.10 */

    float alpha_T;         /* Temperature sensitivity [°C⁻¹]
                            * Range: [0.05, 0.10], default: 0.07 */

    float T0;              /* Reference temperature [°C], default: 20.0 */

    float beta_N;          /* Nitrogen fixation enhancement [m² d g⁻¹]
                            * Range: [0.0, 0.5], default: 0.25 */

    float beta_phi;        /* Aggregate stability enhancement [-]
                            * Range: [0.0, 0.6], default: 0.3 */

    /* Respiration term (D_resp) */
    float R_base;          /* Base respiration rate [g C m⁻² d⁻¹]
                            * Range: [0.05, 1.0], default: 0.3 */

    float Q10;             /* Respiration Q10 coefficient [-]
                            * Range: [1.5, 2.5], default: 2.0 */

    float K_theta_r;       /* Half-saturation for respiration moisture [m³ m⁻³]
                            * Default: 0.08 */
} REGv2_SOM_Params;

/**
 * REGv2_FungalBacterial_Table - F:B ratio priming multiplier lookup table.
 *
 * 8-entry table with hard anchors for explosive SOM gains under fungal
 * dominance. Implements the "Johnson-Su compost" effect.
 *
 * Anchors (non-negotiable):
 *   - F:B = 0.1  → P_Fb = 1.0  (bacterial dominance baseline)
 *   - F:B = 1.0  → P_Fb = 2.5  (critical pivot - Johnson-Su target)
 *   - F:B > 3.0  → P_Fb = 6-8  (extreme fungal dominance, cap at 8.0)
 */
typedef struct {
    float FB_ratio[8];     /* F:B ratio keys */
    float multiplier[8];   /* Priming multiplier values */
} REGv2_FungalBacterial_Table;

/**
 * REGv2_Aggregation_Params - Aggregation-conductivity linkage parameters.
 *
 * Controls the nonlinear effects of microbial aggregation (EPS, glomalin,
 * AMF hyphae) on unsaturated hydraulic conductivity, with threshold behavior.
 */
typedef struct {
    float m_agg;           /* Aggregation multiplier strength [-]
                            * Range: [0.0, 0.6], default: 0.3
                            * Maximum: +60% K in plant-available range */

    float Phi_c;           /* Critical aggregate threshold for connectivity [-]
                            * Range: [0.4, 0.6], default: 0.5
                            * Sigmoid inflection point */

    float gamma;           /* Sigmoid steepness for aggregate threshold [-]
                            * Range: [5.0, 15.0], default: 10.0 */

    float alpha_myco;      /* Mycorrhizal hyphal conductivity enhancement [-]
                            * Range: [0.2, 0.4], default: 0.3
                            * Physical: +20-40% K with functional AMF */

    float theta_rep;       /* Moisture threshold for hydrophobicity [m³ m⁻³]
                            * Range: [0.02, 0.08], default: 0.05
                            * Below this: water repellency reduces infiltration */

    float eta;             /* Sigmoid steepness for repellency [-]
                            * Range: [30.0, 80.0], default: 50.0 */

    float C_thr;           /* Carbon supply threshold for functional symbiosis [g C m⁻² d⁻¹]
                            * Default: 0.5
                            * Gates mycorrhizal conductivity on host carbon */
} REGv2_Aggregation_Params;

/**
 * REGv2_Condensation_Params - Condensation/fog/dew flux parameters.
 *
 * Controls nonrainfall water inputs via vapor condensation, fog interception,
 * and dew formation, including rock-mulch and vegetation multipliers.
 */
typedef struct {
    float Lambda;          /* Vapor transfer coefficient [m s⁻¹]
                            * Range: [1e-5, 5e-4], default: 1e-4
                            * Calibrate to field yields: 0.1-5 mm/d */

    float rho_w;           /* Water density [kg m⁻³], default: 1000.0 */

    float beta_rock;       /* Rock-mulch thermal-mass multiplier [-]
                            * Range: [0.5, 2.0], default: 1.2
                            * M_rock = 1 + β_rock * ΔT_night */

    float beta_veg;        /* Vegetation LAI enhancement coefficient [-]
                            * Range: [0.02, 0.10], default: 0.05
                            * M_veg = 1 + β_veg * LAI */

    float condenser_bonus; /* Neighborhood condenser bonus [mm night⁻¹ per neighbor]
                            * Range: [0.1, 0.5], default: 0.3
                            * Rock mulch, vegetation patch effects */
} REGv2_Condensation_Params;

/**
 * REGv2_BioRain_Params - Biological CCN/INP precipitation bonus parameters.
 *
 * Controls the conditional precipitation probability boost when vegetation
 * cover and fungal dominance exceed thresholds (bioprecipitation feedback).
 */
typedef struct {
    float delta_min;       /* Minimum precipitation probability boost [-]
                            * Range: [0.05, 0.08], default: 0.05 */

    float delta_max;       /* Maximum precipitation probability boost [-]
                            * Range: [0.12, 0.15], default: 0.12 */

    float veg_threshold;   /* Minimum vegetation cover for activation [-]
                            * Default: 0.6 */

    float FB_threshold;    /* Minimum F:B ratio for activation [-]
                            * Default: 2.0 (fungal-dominated) */

    float FB_saturation;   /* F:B ratio at which bonus saturates [-]
                            * Default: 3.0 */
} REGv2_BioRain_Params;

/**
 * REGv2_HydraulicLift_Params - Night-only hydraulic redistribution parameters.
 *
 * Controls the nightly water redistribution from deep to shallow soil layers
 * via root hydraulic lift.
 */
typedef struct {
    float k_root;          /* Root conductance parameter [m s⁻¹]
                            * Range: [1e-8, 5e-6], default: 1e-6
                            * Calibrate to nightly bounds: 0.1-1.3 mm/night */

    float H;               /* Effective rooting depth [m]
                            * Range: [0.3, 3.0], default: 1.5 */

    float Q_lift_min;      /* Lower bound for validation [mm night⁻¹]
                            * Default: 0.1 */

    float Q_lift_max;      /* Upper bound for validation [mm night⁻¹]
                            * Default: 1.3 */

    bool night_gate_active; /* Enforce nighttime-only operation
                             * Default: true */
} REGv2_HydraulicLift_Params;

/**
 * REGv2_Swale_Params - Crescent swale microcatchment parameters.
 *
 * Controls water balance for crescent (half-moon) swales with rock mulch
 * and condensation bonuses.
 */
typedef struct {
    float A_catch;         /* Catchment area multiplier [-]
                            * Default: 3.0 */

    float depress_storage; /* Depression storage depth [m]
                            * Default: 0.005 (5 mm) */

    float L_infiltration;  /* Characteristic length for infiltration gradient [m]
                            * Default: 0.1 */
} REGv2_Swale_Params;

/**
 * REGv2_MicrobialParams - Master parameter struct for REGv2 solver.
 *
 * Aggregates all sub-module parameters. Loaded from REGv2_Microbial.json
 * at initialization.
 */
typedef struct {
    REGv2_SOM_Params som;
    REGv2_FungalBacterial_Table fb_table;
    REGv2_Aggregation_Params aggregation;
    REGv2_Condensation_Params condensation;
    REGv2_BioRain_Params biorain;
    REGv2_HydraulicLift_Params hydraulic_lift;
    REGv2_Swale_Params swale;
} REGv2_MicrobialParams;

/* ========================================================================
 * CORE SOLVER FUNCTIONS (Edison's Pseudocode - Verbatim Implementation)
 * ======================================================================== */

/**
 * Load REGv2 microbial parameters from JSON file.
 *
 * Parses REGv2_Microbial.json and populates the REGv2_MicrobialParams struct.
 * Must be called once at initialization.
 *
 * @param filename      Path to JSON parameter file (REGv2_Microbial.json)
 * @param params        [OUT] Parameter struct to populate
 * @return 0 on success, -1 on error
 */
int regv2_microbial_load_params(const char* filename, REGv2_MicrobialParams* params);

/**
 * F:B multiplier lookup (8-entry table, exact lookup with nearest-neighbor).
 *
 * Implements Edison's pseudocode section 3.1 verbatim:
 *   - Hard anchors: 0.1→1.0x, 1.0→2.5x, >3.0→6-8x
 *   - Cap at 8.0x for extreme Johnson-Su cases (F:B > 1000)
 *
 * @param FB_ratio      Fungal:Bacterial biomass ratio [-]
 * @param table         F:B lookup table (8 entries)
 * @return Priming multiplier P_Fb
 *
 * Performance: O(1) lookup, <5 cycles/cell
 */
float regv2_lookup_P_Fb(float FB_ratio, const REGv2_FungalBacterial_Table* table);

/**
 * Microbial SOM production term with fungal priming.
 *
 * Implements Edison's pseudocode section 3.2:
 *   P_micro = P_max * P_Fb(F:B) * [C_labile/(K_C+C_labile)]
 *             * [θ/(K_θ+θ)] * exp(α_T*(T-T0))
 *             * (1 + β_N*N_fix) * (1 + β_φ*Φ_agg)
 *
 * @param C_labile      Labile carbon pool [g C m⁻²]
 * @param theta         Soil moisture [m³ m⁻³]
 * @param T             Soil temperature [°C]
 * @param N_fix         Nitrogen fixation rate [g N m⁻² d⁻¹]
 * @param Phi_agg       Aggregate stability index [0-1]
 * @param FB_ratio      Fungal:Bacterial ratio [-]
 * @param params        SOM parameters
 * @param table         F:B lookup table
 * @return Production rate [g C m⁻² d⁻¹]
 *
 * Performance: ~15 cycles/cell (bounded Monod-Arrhenius)
 */
float regv2_P_micro(
    float C_labile,
    float theta,
    float T,
    float N_fix,
    float Phi_agg,
    float FB_ratio,
    const REGv2_SOM_Params* params,
    const REGv2_FungalBacterial_Table* table
);

/**
 * Microbial respiration loss term.
 *
 * Implements Edison's pseudocode section 3.2:
 *   D_resp = R_base * Q10^((T-T0)/10) * [θ/(K_θr+θ)] * O2
 *
 * @param T             Soil temperature [°C]
 * @param theta         Soil moisture [m³ m⁻³]
 * @param O2            Oxygen availability [0-1]
 * @param params        SOM parameters
 * @return Respiration rate [g C m⁻² d⁻¹]
 *
 * Performance: ~8 cycles/cell (Q10 formulation with moisture gate)
 */
float regv2_D_resp(
    float T,
    float theta,
    float O2,
    const REGv2_SOM_Params* params
);

/**
 * Unsaturated hydraulic conductivity with aggregation and mycorrhizal effects.
 *
 * Implements Edison's pseudocode section 3.3:
 *   K(θ) = K0(θ) * [1 + m_agg*Φ_agg*S(Φ_agg)] * [1 + α_myco*Φ_hyphae*σ(C_sup)] * R(θ)
 *
 * Where:
 *   S(Φ_agg) = sigmoid threshold for aggregate connectivity
 *   R(θ) = moisture-dependent repellency factor
 *   σ(C_sup) = carbon supply gate for functional symbiosis
 *
 * @param theta         Soil moisture [m³ m⁻³]
 * @param K0_theta      Base hydraulic conductivity at θ [m s⁻¹]
 * @param Phi_agg       Aggregate stability index [0-1]
 * @param Phi_hyphae    Hyphal density index [0-1]
 * @param C_sup         Host carbon supply [g C m⁻² d⁻¹]
 * @param params        Aggregation parameters
 * @return Effective conductivity K(θ) [m s⁻¹]
 *
 * Performance: ~12 cycles/cell (3 sigmoids + multipliers)
 */
float regv2_K_unsat(
    float theta,
    float K0_theta,
    float Phi_agg,
    float Phi_hyphae,
    float C_sup,
    const REGv2_Aggregation_Params* params
);

/**
 * Condensation flux with rock-mulch and neighborhood bonuses.
 *
 * Implements Edison's pseudocode section 3.4:
 *   C_cond = ρ_w * Λ * (RH - RH_sat)⁺ * f_r * M_rock * M_veg + bonus
 *   M_rock = 1 + β_rock * ΔT_night
 *   bonus = condenser_bonus * n_cond_neighbors * ΔT_night
 *
 * @param RH            Relative humidity [0-1]
 * @param RH_sat_Ts     Saturation RH at surface temperature [0-1]
 * @param u_star        Friction velocity [m s⁻¹]
 * @param z0            Roughness length [m]
 * @param alpha_surf    Surface emissivity/albedo factor [-]
 * @param dT_night      Nighttime radiative cooling [K]
 * @param LAI           Leaf area index [-]
 * @param n_cond_neighbors Number of condenser neighbors (rock mulch, vegetation)
 * @param params        Condensation parameters
 * @return Condensation flux [mm h⁻¹ equivalent]
 *
 * Performance: ~10 cycles/cell (vapor excess + multipliers)
 */
float regv2_C_cond(
    float RH,
    float RH_sat_Ts,
    float u_star,
    float z0,
    float alpha_surf,
    float dT_night,
    float LAI,
    int n_cond_neighbors,
    const REGv2_Condensation_Params* params
);

/**
 * Biological CCN/INP precipitation probability bonus.
 *
 * Implements Edison's pseudocode section 3.5:
 *   If V > 0.6 AND F:B ≥ 2.0:
 *     Δp_bio = δ_min + (δ_max - δ_min) * [(F:B - 2)/(3 - 2)]
 *   Else:
 *     Δp_bio = 0
 *
 * @param veg_cover     Vegetation cover fraction [0-1]
 * @param FB_ratio      Fungal:Bacterial ratio [-]
 * @param params        BioRain parameters
 * @return Precipitation probability increment [0.05-0.15]
 *
 * Performance: ~5 cycles/cell (threshold check + linear interpolation)
 */
float regv2_bio_rain_bonus(
    float veg_cover,
    float FB_ratio,
    const REGv2_BioRain_Params* params
);

/**
 * Hydraulic lift flux (night-only root water redistribution).
 *
 * Implements Edison's pseudocode section 3.6:
 *   Q_lift = k_root * (θ_deep - θ_shallow) * H * χ_night
 *
 * Where χ_night gates operation to nighttime hours.
 * Nightly integration must satisfy bounds: 0.1-1.3 mm H2O night⁻¹
 *
 * @param theta_deep    Deep soil moisture [m³ m⁻³]
 * @param theta_shallow Shallow soil moisture [m³ m⁻³]
 * @param H             Rooting depth [m]
 * @param is_night      Nighttime gate (0 or 1)
 * @param params        Hydraulic lift parameters
 * @return Lift flux [m s⁻¹]
 *
 * Performance: ~5 cycles/cell (gradient * conductance * gate)
 */
float regv2_Q_lift(
    float theta_deep,
    float theta_shallow,
    float H,
    int is_night,
    const REGv2_HydraulicLift_Params* params
);

/**
 * Update crescent swale water balance.
 *
 * Implements Edison's pseudocode section 3.7:
 *   dS_swale/dt = Q_runon * A_catch - I_swale - E_surf + C_cond_swale
 *   I_swale = K(θ) * (h_pond / L)
 *
 * @param S_swale       [IN/OUT] Swale storage [m³]
 * @param Q_runon       Runon flux [m s⁻¹]
 * @param I_swale       [OUT] Infiltration flux [m s⁻¹]
 * @param E_surf        Surface evaporation [m s⁻¹]
 * @param C_cond_swale  Condensation into swale [m s⁻¹]
 * @param K_theta       Hydraulic conductivity at current θ [m s⁻¹]
 * @param area          Swale area [m²]
 * @param dt            Timestep [s]
 * @param params        Swale parameters
 *
 * Performance: ~8 cycles/cell (water balance update + infiltration)
 */
void regv2_update_swale(
    float* S_swale,
    float Q_runon,
    float* I_swale,
    float E_surf,
    float C_cond_swale,
    float K_theta,
    float area,
    float dt,
    const REGv2_Swale_Params* params
);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * Sigmoid function (for thresholds).
 *
 * @param x Input
 * @return 1 / (1 + exp(-x))
 */
static inline float regv2_sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * Positive-part function (max(0, x)).
 *
 * @param x Input
 * @return max(0, x)
 */
static inline float regv2_positive_part(float x) {
    return (x > 0.0f) ? x : 0.0f;
}

#endif /* REGENERATION_MICROBIAL_H */
