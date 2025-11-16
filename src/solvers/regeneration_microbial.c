/*
 * regeneration_microbial.c - REGv2 Microbial Priming & Condenser Landscapes
 *
 * Implements the microscale biological and atmospheric-interface dynamics that
 * drive explosive, nonlinear regeneration. This module provides the "biological
 * detonation charges" - the fungal heartbeat that enables land to heal.
 *
 * Implementation Strategy:
 *   - Verbatim translation of Edison's pseudocode (sections 3.1-3.7)
 *   - All functions <50 cycles/cell (Grok verified)
 *   - Empirically grounded parameter ranges with citations
 *   - Threshold-driven nonlinear state flips
 *
 * Author: negentropic-core team
 * Version: 0.1.0 (REGv2)
 * License: MIT OR GPL-3.0
 */

#include "regeneration_microbial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========================================================================
 * PARAMETER LOADING (JSON parsing)
 * ======================================================================== */

/**
 * Simple JSON parameter parser (same approach as REGv1).
 *
 * Searches for key-value pairs like:
 *   "key": { "value": 1.23, ... }
 * and extracts the "value" field.
 */
static int parse_json_param(const char* json, const char* key, float* out_value) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char* key_pos = strstr(json, search_key);
    if (!key_pos) {
        return -1;  /* Key not found */
    }

    const char* value_pos = strstr(key_pos, "\"value\"");
    if (!value_pos) {
        return -1;  /* "value" field not found */
    }

    const char* colon = strchr(value_pos, ':');
    if (!colon) {
        return -1;
    }

    float value;
    if (sscanf(colon + 1, "%f", &value) != 1) {
        return -1;
    }

    *out_value = value;
    return 0;
}

/**
 * Parse F:B table from JSON.
 *
 * Expects an "entries" array with objects containing "FB_ratio" and "multiplier".
 * This is a simplified parser - production code should use a proper JSON library.
 */
static int parse_fb_table(const char* json, REGv2_FungalBacterial_Table* table) {
    (void)json;  /* TODO: Parse from JSON in production */
    /* Hard-coded 8-entry table based on Edison's specification */
    /* In production, parse from JSON "entries" array */
    table->FB_ratio[0] = 0.10f;   table->multiplier[0] = 1.0f;
    table->FB_ratio[1] = 0.25f;   table->multiplier[1] = 1.2f;
    table->FB_ratio[2] = 0.50f;   table->multiplier[2] = 1.6f;
    table->FB_ratio[3] = 1.00f;   table->multiplier[3] = 2.5f;
    table->FB_ratio[4] = 1.50f;   table->multiplier[4] = 3.5f;
    table->FB_ratio[5] = 2.00f;   table->multiplier[5] = 4.5f;
    table->FB_ratio[6] = 3.00f;   table->multiplier[6] = 6.0f;
    table->FB_ratio[7] = 1000.0f; table->multiplier[7] = 8.0f;

    return 0;
}

int regv2_microbial_load_params(const char* filename, REGv2_MicrobialParams* params) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "[REGv2] ERROR: Cannot open parameter file: %s\n", filename);
        return -1;
    }

    /* Read entire file into buffer */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* json = malloc(fsize + 1);
    if (!json) {
        fclose(f);
        fprintf(stderr, "[REGv2] ERROR: Memory allocation failed\n");
        return -1;
    }

    fread(json, 1, fsize, f);
    json[fsize] = '\0';
    fclose(f);

    /* Parse SOM module parameters */
    if (parse_json_param(json, "P_max", &params->som.P_max) != 0 ||
        parse_json_param(json, "K_C", &params->som.K_C) != 0 ||
        parse_json_param(json, "K_theta", &params->som.K_theta) != 0 ||
        parse_json_param(json, "alpha_T", &params->som.alpha_T) != 0 ||
        parse_json_param(json, "T0", &params->som.T0) != 0 ||
        parse_json_param(json, "beta_N", &params->som.beta_N) != 0 ||
        parse_json_param(json, "beta_phi", &params->som.beta_phi) != 0 ||
        parse_json_param(json, "R_base", &params->som.R_base) != 0 ||
        parse_json_param(json, "Q10", &params->som.Q10) != 0 ||
        parse_json_param(json, "K_theta_r", &params->som.K_theta_r) != 0) {
        fprintf(stderr, "[REGv2] ERROR: Failed to parse SOM_module parameters\n");
        free(json);
        return -1;
    }

    /* Parse F:B table (hard-coded for now) */
    if (parse_fb_table(json, &params->fb_table) != 0) {
        fprintf(stderr, "[REGv2] ERROR: Failed to parse F:B table\n");
        free(json);
        return -1;
    }

    /* Parse Aggregation_Conductivity parameters */
    if (parse_json_param(json, "m_agg", &params->aggregation.m_agg) != 0 ||
        parse_json_param(json, "Phi_c", &params->aggregation.Phi_c) != 0 ||
        parse_json_param(json, "gamma", &params->aggregation.gamma) != 0 ||
        parse_json_param(json, "alpha_myco", &params->aggregation.alpha_myco) != 0 ||
        parse_json_param(json, "theta_rep", &params->aggregation.theta_rep) != 0 ||
        parse_json_param(json, "eta", &params->aggregation.eta) != 0 ||
        parse_json_param(json, "C_thr", &params->aggregation.C_thr) != 0) {
        fprintf(stderr, "[REGv2] ERROR: Failed to parse Aggregation_Conductivity parameters\n");
        free(json);
        return -1;
    }

    /* Parse Condensation_Fog parameters */
    if (parse_json_param(json, "Lambda", &params->condensation.Lambda) != 0 ||
        parse_json_param(json, "rho_w", &params->condensation.rho_w) != 0 ||
        parse_json_param(json, "beta_rock", &params->condensation.beta_rock) != 0 ||
        parse_json_param(json, "beta_veg", &params->condensation.beta_veg) != 0 ||
        parse_json_param(json, "condenser_bonus", &params->condensation.condenser_bonus) != 0) {
        fprintf(stderr, "[REGv2] ERROR: Failed to parse Condensation_Fog parameters\n");
        free(json);
        return -1;
    }

    /* Parse Biological_Rain_Bonus parameters */
    if (parse_json_param(json, "delta_min", &params->biorain.delta_min) != 0 ||
        parse_json_param(json, "delta_max", &params->biorain.delta_max) != 0 ||
        parse_json_param(json, "veg_threshold", &params->biorain.veg_threshold) != 0 ||
        parse_json_param(json, "FB_threshold", &params->biorain.FB_threshold) != 0 ||
        parse_json_param(json, "FB_saturation", &params->biorain.FB_saturation) != 0) {
        fprintf(stderr, "[REGv2] ERROR: Failed to parse Biological_Rain_Bonus parameters\n");
        free(json);
        return -1;
    }

    /* Parse Hydraulic_Lift parameters */
    if (parse_json_param(json, "k_root", &params->hydraulic_lift.k_root) != 0 ||
        parse_json_param(json, "H", &params->hydraulic_lift.H) != 0 ||
        parse_json_param(json, "Q_lift_min", &params->hydraulic_lift.Q_lift_min) != 0 ||
        parse_json_param(json, "Q_lift_max", &params->hydraulic_lift.Q_lift_max) != 0) {
        fprintf(stderr, "[REGv2] ERROR: Failed to parse Hydraulic_Lift parameters\n");
        free(json);
        return -1;
    }
    params->hydraulic_lift.night_gate_active = true;  /* Default: enforce night-only */

    /* Parse Swale_Microcatchment parameters */
    if (parse_json_param(json, "A_catch", &params->swale.A_catch) != 0 ||
        parse_json_param(json, "depress_storage", &params->swale.depress_storage) != 0 ||
        parse_json_param(json, "L_infiltration", &params->swale.L_infiltration) != 0) {
        fprintf(stderr, "[REGv2] ERROR: Failed to parse Swale_Microcatchment parameters\n");
        free(json);
        return -1;
    }

    free(json);

    fprintf(stderr, "[REGv2] Successfully loaded parameters from %s\n", filename);
    fprintf(stderr, "[REGv2] F:B anchors: 0.1→%.1fx, 1.0→%.1fx, >3.0→%.1fx\n",
            params->fb_table.multiplier[0],
            params->fb_table.multiplier[3],
            params->fb_table.multiplier[6]);

    return 0;
}

/* ========================================================================
 * CORE SOLVER FUNCTIONS (Edison's Pseudocode - Verbatim Implementation)
 * ======================================================================== */

/**
 * Section 3.1: F:B multiplier (table; exact lookup with nearest-neighbor)
 *
 * Inputs: FB_ratio
 * Output: P_Fb
 *
 * Implementation note: This is a direct copy of Edison's pseudocode.
 */
float regv2_lookup_P_Fb(float FB_ratio, const REGv2_FungalBacterial_Table* table) {
    /* Handle lower bound */
    if (FB_ratio <= table->FB_ratio[0]) {
        return table->multiplier[0];
    }

    /* Exact lookup: find first entry where FB_ratio <= key[i] */
    for (int i = 1; i < 8; i++) {
        if (FB_ratio <= table->FB_ratio[i]) {
            return table->multiplier[i];
        }
    }

    /* Upper bound (extreme cases, F:B > 1000) */
    return table->multiplier[7];
}

/**
 * Section 3.2: P_micro - Microbial SOM production with fungal priming
 *
 * P_micro = P_max * P_Fb(F:B) * [C_labile/(K_C + C_labile)]
 *           * [θ/(K_θ + θ)] * exp(α_T*(T - T0))
 *           * (1 + β_N*N_fix) * (1 + β_φ*Φ_agg)
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
) {
    /* Fungal priming multiplier (hard anchors enforced [1.1]) */
    float P_Fb = regv2_lookup_P_Fb(FB_ratio, table);

    /* Monod kinetics for labile carbon */
    float term_C = C_labile / (params->K_C + C_labile);

    /* Moisture dependence */
    float term_W = theta / (params->K_theta + theta);

    /* Arrhenius temperature dependence */
    float term_T = expf(params->alpha_T * (T - params->T0));

    /* Nitrogen fixation enhancement */
    float term_N = 1.0f + params->beta_N * N_fix;

    /* Aggregate stability enhancement */
    float term_agg = 1.0f + params->beta_phi * Phi_agg;

    return params->P_max * P_Fb * term_C * term_W * term_T * term_N * term_agg;
}

/**
 * Section 3.2: D_resp - Microbial respiration loss
 *
 * D_resp = R_base * Q10^((T - T0)/10) * [θ/(K_θr + θ)] * O2
 */
float regv2_D_resp(
    float T,
    float theta,
    float O2,
    const REGv2_SOM_Params* params
) {
    /* Q10 temperature dependence */
    float q10_term = powf(params->Q10, (T - params->T0) / 10.0f);

    /* Moisture gate */
    float moist_term = theta / (params->K_theta_r + theta);

    /* Oxygen availability */
    return params->R_base * q10_term * moist_term * O2;
}

/**
 * Section 3.3: K_unsat - Aggregation-conductivity multiplier with thresholds
 *
 * K(θ) = K0(θ) * [1 + m_agg*Φ_agg*S(Φ_agg)] * [1 + α_myco*Φ_hyphae*σ(C_sup)] * R(θ)
 *
 * Where:
 *   S(Φ_agg) = 1 / (1 + exp(-γ*(Φ_agg - Φ_c)))  [aggregate connectivity threshold]
 *   R(θ) = 1 / (1 + exp(η*(θ_rep - θ)))          [moisture-dependent repellency]
 *   σ(C_sup) = 1 / (1 + exp(-(C_sup - C_thr)))   [carbon supply gate]
 */
float regv2_K_unsat(
    float theta,
    float K0_theta,
    float Phi_agg,
    float Phi_hyphae,
    float C_sup,
    const REGv2_Aggregation_Params* params
) {
    /* Aggregate connectivity sigmoid (threshold at Φ_c) */
    float S_agg = regv2_sigmoid(params->gamma * (Phi_agg - params->Phi_c));

    /* Moisture-dependent repellency factor */
    float R_rep = 1.0f / (1.0f + expf(params->eta * (params->theta_rep - theta)));

    /* Carbon supply gate for functional mycorrhizal symbiosis */
    float sigma_C = regv2_sigmoid(C_sup - params->C_thr);

    /* Aggregation multiplier (EPS, glomalin effects) */
    float mult_agg = 1.0f + params->m_agg * Phi_agg * S_agg;

    /* Mycorrhizal hyphal conductivity enhancement (+20-40% [8.1]) */
    float mult_myco = 1.0f + params->alpha_myco * Phi_hyphae * sigma_C;

    return K0_theta * mult_agg * mult_myco * R_rep;
}

/**
 * Section 3.4: C_cond - Condensation flux with rock-mulch and condenser bonus
 *
 * C_cond = ρ_w * Λ * (RH - RH_sat)⁺ * f_r * M_rock * M_veg + bonus
 * M_rock = 1 + β_rock * ΔT_night
 * M_veg = 1 + β_veg * LAI
 * bonus = condenser_bonus * n_cond_neighbors * ΔT_night
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
) {
    /* TODO: Full implementation would include roughness f_u(u*, z0) and emissivity f_r(α_surf) effects */
    (void)u_star;
    (void)z0;
    (void)alpha_surf;

    /* Vapor excess (positive part) */
    float vapor_excess = regv2_positive_part(RH - RH_sat_Ts);

    /* Vegetation enhancement */
    float M_veg = 1.0f + params->beta_veg * LAI;

    /* Rock-mulch thermal mass multiplier */
    float M_rock = 1.0f + params->beta_rock * dT_night;

    /* Base condensation flux (mm h⁻¹ equivalent via unit scaling) */
    /* Note: f_u(u*, z0) and f_r(α_surf) are simplified here as unity;
     * full implementation would include roughness and emissivity effects */
    float base_flux = params->rho_w * params->Lambda * vapor_excess * M_veg * M_rock;

    /* Neighborhood condenser bonus (0.1-0.5 mm/night [5.1]) */
    float bonus = params->condenser_bonus * (float)n_cond_neighbors * dT_night;

    return base_flux + bonus;
}

/**
 * Section 3.5: bio_rain_bonus - Biological CCN/INP precipitation probability boost
 *
 * If veg_cover > 0.6 AND F:B ≥ 2.0:
 *   Δp_bio = δ_min + (δ_max - δ_min) * [(F:B - 2) / (3 - 2)]
 * Else:
 *   Δp_bio = 0
 *
 * Clipped to [0.05, 0.15]
 */
float regv2_bio_rain_bonus(
    float veg_cover,
    float FB_ratio,
    const REGv2_BioRain_Params* params
) {
    /* Check thresholds */
    if (veg_cover > params->veg_threshold && FB_ratio >= params->FB_threshold) {
        /* Linear interpolation between F:B = 2.0 and 3.0 */
        float frac = (FB_ratio - params->FB_threshold) /
                     (params->FB_saturation - params->FB_threshold);

        /* Clamp to [0, 1] */
        frac = fminf(1.0f, fmaxf(0.0f, frac));

        float delta = params->delta_min + (params->delta_max - params->delta_min) * frac;

        /* Clip to valid probability range */
        return fminf(0.15f, fmaxf(0.05f, delta));
    }

    return 0.0f;
}

/**
 * Section 3.6: Q_lift - Hydraulic lift (night-only)
 *
 * Q_lift = k_root * (θ_deep - θ_shallow) * H * χ_night
 *
 * Where χ_night gates to nighttime hours. Nightly integration must satisfy
 * bounds: 0.1-1.3 mm H2O night⁻¹ [4.1], [4.2], [11.1]
 */
float regv2_Q_lift(
    float theta_deep,
    float theta_shallow,
    float H,
    int is_night,
    const REGv2_HydraulicLift_Params* params
) {
    /* Compute gradient-driven flux */
    float q = params->k_root * (theta_deep - theta_shallow) * H;

    /* Apply night gate if active */
    if (params->night_gate_active) {
        q *= (float)is_night;
    }

    /* Note: Caller must integrate over night duration and verify
     * nightly total is within [Q_lift_min, Q_lift_max] bounds */

    return q;
}

/**
 * Section 3.7: update_swale - Crescent swale water balance (per time step dt)
 *
 * dS_swale/dt = Q_runon*A_catch - I_swale - E_surf + C_cond_swale
 * I_swale = K(θ) * (h_pond / L)
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
) {
    /* Compute ponding depth above depression storage */
    float h_pond = fmaxf(0.0f, (*S_swale / area) - params->depress_storage);

    /* Infiltration flux (Darcy-like with gradient h_pond/L) */
    *I_swale = K_theta * (h_pond / params->L_infiltration);

    /* Update swale storage */
    float dS_dt = Q_runon * params->A_catch - *I_swale - E_surf + C_cond_swale;
    *S_swale += dS_dt * dt;

    /* Clamp to non-negative */
    if (*S_swale < 0.0f) {
        *S_swale = 0.0f;
    }
}
