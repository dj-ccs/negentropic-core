/*
 * regeneration_cascade.c - Regeneration Cascade Solver (REGv1) Implementation
 *
 * Implements the slow-timescale vegetation-SOM-moisture feedback loop.
 * Uses fixed-point 16.16 arithmetic for performance-critical calculations.
 *
 * REGv2 Integration:
 *   SOM dynamics now use microbial priming functions from REGv2 module.
 *   This enables explosive, nonlinear recovery through fungal dominance.
 *
 * Author: negentropic-core team
 * Version: 0.2.0 (REGv1 with REGv2 integration)
 * License: MIT OR GPL-3.0
 */

#include "regeneration_cascade.h"
#include "regeneration_microbial.h"  /* REGv2 integration */
#include "hydrology_richards_lite.h"
#include "hydrology_richards_lite_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Global REGv2 parameters (loaded at initialization) */
static REGv2_MicrobialParams g_regv2_params;
static int g_regv2_enabled = 0;  /* Flag: 0 = use old simple SOM, 1 = use REGv2 */

/* ========================================================================
 * FIXED-POINT ARITHMETIC MACROS (16.16 format)
 * ======================================================================== */

#define FRACBITS 16
#define FRACUNIT (1 << FRACBITS)  /* 65,536 */

/* Conversion macros */
#define FLOAT_TO_FXP(f)   ((int32_t)((f) * FRACUNIT))
#define FXP_TO_FLOAT(fxp) ((float)(fxp) / FRACUNIT)
#define INT_TO_FXP(i)     ((int32_t)((i) << FRACBITS))

/* Fixed-point multiply: (a * b) >> 16 */
static inline int32_t fxp_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> FRACBITS);
}

/* Fixed-point divide: (a << 16) / b */
static inline int32_t fxp_div(int32_t a, int32_t b) {
    if (b == 0) {
        return (a < 0) ? INT32_MIN : INT32_MAX;
    }
    return (int32_t)(((int64_t)a << FRACBITS) / (int64_t)b);
}

/* ========================================================================
 * PARAMETER LOADING (JSON parsing)
 * ======================================================================== */

/**
 * Simple JSON parameter parser.
 *
 * IMPORTANT: This is a minimal implementation for loading flat numeric
 * parameters. It does NOT handle nested objects or arrays. For production,
 * consider using a proper JSON library (e.g., cJSON, jsmn).
 *
 * This function searches for key-value pairs like:
 *   "key": { "value": 1.23, ... }
 * and extracts the "value" field.
 */
static int parse_json_param(const char* json, const char* key, float* out_value) {
    /* Find the key in the JSON string */
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char* key_pos = strstr(json, search_key);
    if (!key_pos) {
        return -1;  /* Key not found */
    }

    /* Find the "value" field within this key's object */
    const char* value_pos = strstr(key_pos, "\"value\"");
    if (!value_pos) {
        return -1;  /* "value" field not found */
    }

    /* Find the colon after "value" */
    const char* colon = strchr(value_pos, ':');
    if (!colon) {
        return -1;
    }

    /* Parse the numeric value */
    float value;
    if (sscanf(colon + 1, "%f", &value) != 1) {
        return -1;  /* Failed to parse number */
    }

    *out_value = value;
    return 0;  /* Success */
}

int regeneration_cascade_load_params(const char* filename, RegenerationParams* params) {
    /* Open file */
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "[REGv1] ERROR: Cannot open parameter file: %s\n", filename);
        return -1;
    }

    /* Read entire file into buffer */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* json = (char*)malloc(file_size + 1);
    if (!json) {
        fprintf(stderr, "[REGv1] ERROR: Cannot allocate memory for JSON\n");
        fclose(f);
        return -1;
    }

    size_t read_size = fread(json, 1, file_size, f);
    json[read_size] = '\0';
    fclose(f);

    /* Parse parameters */
    int errors = 0;

    errors += parse_json_param(json, "r_V", &params->r_V);
    errors += parse_json_param(json, "K_V", &params->K_V);
    errors += parse_json_param(json, "lambda1", &params->lambda1);
    errors += parse_json_param(json, "lambda2", &params->lambda2);
    errors += parse_json_param(json, "theta_star", &params->theta_star);
    errors += parse_json_param(json, "SOM_star", &params->SOM_star);
    errors += parse_json_param(json, "a1", &params->a1);
    errors += parse_json_param(json, "a2", &params->a2);
    errors += parse_json_param(json, "eta1", &params->eta1);
    errors += parse_json_param(json, "K_vertical_multiplier", &params->K_vertical_multiplier);

    free(json);

    if (errors != 0) {
        fprintf(stderr, "[REGv1] ERROR: Failed to parse %d parameters from %s\n", errors, filename);
        return -1;
    }

    /* Validate ranges */
    if (params->r_V <= 0.0f || params->r_V > 1.0f) {
        fprintf(stderr, "[REGv1] ERROR: Invalid r_V = %f (expected 0 < r_V < 1)\n", params->r_V);
        return -1;
    }
    if (params->K_V <= 0.0f || params->K_V > 1.0f) {
        fprintf(stderr, "[REGv1] ERROR: Invalid K_V = %f (expected 0 < K_V < 1)\n", params->K_V);
        return -1;
    }

    fprintf(stderr, "[REGv1] Loaded parameters from %s:\n", filename);
    fprintf(stderr, "  r_V=%.3f, K_V=%.3f, λ1=%.3f, λ2=%.3f\n",
            params->r_V, params->K_V, params->lambda1, params->lambda2);
    fprintf(stderr, "  θ*=%.3f, SOM*=%.3f, a1=%.3f, a2=%.3f\n",
            params->theta_star, params->SOM_star, params->a1, params->a2);
    fprintf(stderr, "  η1=%.3f mm/%%SOM, K_mult=%.3f\n",
            params->eta1, params->K_vertical_multiplier);

    return 0;  /* Success */
}

/* ========================================================================
 * REGv2 INTEGRATION
 * ======================================================================== */

/**
 * Load REGv2 microbial parameters and enable enhanced SOM dynamics.
 *
 * Call this function at initialization to enable the REGv2 microbial priming
 * module. If not called, REGv1 will use the simple linear SOM model.
 *
 * @param filename Path to REGv2_Microbial.json
 * @return 0 on success, -1 on error
 */
int regeneration_cascade_enable_regv2(const char* filename) {
    if (regv2_microbial_load_params(filename, &g_regv2_params) != 0) {
        fprintf(stderr, "[REGv1] ERROR: Failed to load REGv2 parameters\n");
        return -1;
    }

    g_regv2_enabled = 1;
    fprintf(stderr, "[REGv1] REGv2 microbial priming ENABLED\n");
    fprintf(stderr, "[REGv1] SOM dynamics will use fungal-bacterial priming with explosive recovery\n");

    return 0;
}

/* ========================================================================
 * CORE SOLVER IMPLEMENTATION
 * ======================================================================== */

/**
 * Apply hydrological bonus to a single cell.
 *
 * This implements the critical coupling mechanism from REGv1 to HYD-RLv1.
 * SOM changes modify the soil's hydrological properties:
 *   - porosity_eff: +1% SOM → +η1 mm water holding capacity
 *   - K_tensor[8]: +1% SOM → ×K_mult vertical conductivity (compounding)
 *
 * Physical basis:
 *   - SOM increases soil structure → more pore space (porosity)
 *   - SOM creates macropores → higher vertical infiltration (K_zz)
 *
 * @param c Cell to modify
 * @param dSOM Change in SOM [%] (can be negative)
 * @param params Parameter set (for eta1, K_vertical_multiplier)
 */
static inline void apply_hydrological_bonus(
    Cell* c,
    float dSOM,
    const RegenerationParams* params
) {
    /* Rule 1: +1% SOM adds +η1 mm of water holding capacity */
    /* Convert: η1 [mm/%SOM] → porosity [m³/m³]
     * Assumption: 1m soil depth, so η1 mm = η1/1000 m */
    float porosity_bonus = (params->eta1 / 1000.0f) * dSOM;
    c->porosity_eff += porosity_bonus;

    /* Clamp porosity to physical range [0.3, 0.7] */
    if (c->porosity_eff < 0.3f) c->porosity_eff = 0.3f;
    if (c->porosity_eff > 0.7f) c->porosity_eff = 0.7f;

    /* Rule 2: +1% SOM gives a +K_mult% compounding bonus to vertical conductivity */
    /* K_zz *= (K_mult)^dSOM
     * For K_mult = 1.15 and dSOM = 1.0: K_zz *= 1.15 (15% increase) */
    if (dSOM != 0.0f) {
        float K_multiplier = powf(params->K_vertical_multiplier, dSOM);
        c->K_tensor[8] *= K_multiplier;  /* K_tensor[8] is K_zz (vertical) */

        /* Clamp K_zz to reasonable range [1e-8, 1e-3] m/s */
        if (c->K_tensor[8] < 1e-8f) c->K_tensor[8] = 1e-8f;
        if (c->K_tensor[8] > 1e-3f) c->K_tensor[8] = 1e-3f;
    }
}

/**
 * Single-cell regeneration step (the core ODE solver).
 *
 * Solves the coupled vegetation-SOM ODEs:
 *   dV/dt = r_V * V * (1 - V/K_V) + λ1 * max(θ - θ*, 0) + λ2 * max(SOM - SOM*, 0)
 *   dSOM/dt = a1 * V - a2 * SOM
 *
 * Uses fixed-point arithmetic for V and SOM calculations.
 *
 * @param c Cell to update
 * @param theta_avg Average soil moisture [m³/m³] from vertical profile
 * @param params Parameter set
 * @param dt_years Timestep [years]
 */
static inline void regeneration_cascade_step_single_cell(
    Cell* c,
    float theta_avg,
    const RegenerationParams* params,
    float dt_years
) {
    /* --- 1. Read State & Convert from Fixed-Point to Float for ODEs --- */
    float V   = FXP_TO_FLOAT(c->vegetation_cover_fxp);
    float SOM = FXP_TO_FLOAT(c->SOM_percent_fxp);

    /* --- 2. Solve the Coupled ODEs --- */

    /* Vegetation ODE: dV/dt = r_V * V * (1 - V/K_V) + λ1 * max(θ - θ*, 0) + λ2 * max(SOM - SOM*, 0) */
    float logistic_term = params->r_V * V * (1.0f - V / params->K_V);
    float moisture_term = params->lambda1 * fmaxf(theta_avg - params->theta_star, 0.0f);
    float som_term = params->lambda2 * fmaxf(SOM - params->SOM_star, 0.0f);

    float dV = logistic_term + moisture_term + som_term;

    /* SOM ODE: Choose between simple model (REGv1) or microbial priming (REGv2) */
    float dSOM;
    if (g_regv2_enabled) {
        /* REGv2: Microbial priming with fungal-bacterial dynamics
         * dSOM/dt = P_micro(...) - D_resp(...) */

        /* Compute production term with fungal priming */
        float P_production = regv2_P_micro(
            c->C_labile,
            theta_avg,
            c->soil_temp_C,
            c->N_fix,
            c->Phi_agg,
            c->FB_ratio,
            &g_regv2_params.som,
            &g_regv2_params.fb_table
        );

        /* Compute respiration loss */
        float D_loss = regv2_D_resp(
            c->soil_temp_C,
            theta_avg,
            c->O2,
            &g_regv2_params.som
        );

        /* Net SOM change: production - respiration
         * NOTE: Need to convert from [g C m⁻² d⁻¹] to [% SOM yr⁻¹]
         * Assuming: 1% SOM ≈ 100 g C m⁻² (typical conversion)
         * So: [g C m⁻² d⁻¹] × (365.25 d/yr) / (100 g C m⁻² per %SOM) = [% SOM yr⁻¹] */
        const float carbon_to_SOM_conversion = 365.25f / 100.0f;  /* days/yr ÷ (g C m⁻² per %SOM) */
        dSOM = (P_production - D_loss) * carbon_to_SOM_conversion;

    } else {
        /* REGv1: Simple linear model (original) */
        dSOM = params->a1 * V - params->a2 * SOM;
    }

    /* --- 3. Update State & Convert back to Fixed-Point --- */
    float next_V   = V + dV * dt_years;
    float next_SOM = SOM + dSOM * dt_years;

    /* Clamp V to [0, 1] and SOM to [0.01, 10.0] */
    next_V = fminf(fmaxf(next_V, 0.0f), 1.0f);
    next_SOM = fmaxf(next_SOM, 0.01f);  /* Prevent SOM from going to zero */
    if (next_SOM > 10.0f) next_SOM = 10.0f;  /* Upper bound for physical realism */

    c->vegetation_cover_fxp = FLOAT_TO_FXP(next_V);
    c->SOM_percent_fxp      = FLOAT_TO_FXP(next_SOM);

    /* Sync float versions for easy inspection/debugging */
    c->vegetation_cover = next_V;
    c->SOM_percent      = next_SOM;

    /* --- 4. Apply the Hydrological Bonus (see Section 5) --- */
    apply_hydrological_bonus(c, dSOM * dt_years, params);
}

void regeneration_cascade_step(
    Cell* grid,
    size_t grid_size,
    const RegenerationParams* params,
    float dt_years
) {
    /* CRITICAL ASSUMPTION:
     * The grid is structured as a 1D array of cells representing either:
     *   (a) A flat 2D grid (nx × ny) with single-layer moisture, OR
     *   (b) A 3D grid (nx × ny × nz) where we process only the top layer
     *
     * For case (b), the caller must ensure grid_size = nx * ny (not nx * ny * nz),
     * and theta_avg must be pre-computed externally.
     *
     * For simplicity in v1.0, we assume case (a): each cell has a single theta value.
     */

    for (size_t i = 0; i < grid_size; i++) {
        Cell* c = &grid[i];

        /* For single-layer grids, theta_avg = theta */
        float theta_avg = c->theta;

        /* TODO: For multi-layer grids, calculate theta_avg from vertical profile:
         *   float theta_profile[3] = {c->theta, c[nx*ny].theta, c[2*nx*ny].theta};
         *   float theta_avg = calculate_theta_avg(theta_profile, 3);
         */

        /* Execute single-cell step */
        regeneration_cascade_step_single_cell(c, theta_avg, params, dt_years);
    }
}

/* ========================================================================
 * DIAGNOSTIC FUNCTIONS
 * ======================================================================== */

int regeneration_cascade_threshold_status(
    const Cell* cell,
    const RegenerationParams* params
) {
    int status = 0;

    /* Bit 0: θ > θ* (moisture above critical) */
    if (cell->theta > params->theta_star) {
        status |= (1 << 0);
    }

    /* Bit 1: SOM > SOM* (organic matter above critical) */
    if (cell->SOM_percent > params->SOM_star) {
        status |= (1 << 1);
    }

    /* Bit 2: V > 0.5*K_V (vegetation above midpoint) */
    if (cell->vegetation_cover > 0.5f * params->K_V) {
        status |= (1 << 2);
    }

    return status;
}

float regeneration_cascade_health_score(
    const Cell* cell,
    const RegenerationParams* params
) {
    /* Weights for health score components */
    const float w_V = 0.4f;      /* Vegetation: 40% weight */
    const float w_SOM = 0.35f;   /* SOM: 35% weight */
    const float w_theta = 0.25f; /* Moisture: 25% weight */

    /* Normalize each component to [0, 1] */
    float V_norm = cell->vegetation_cover / params->K_V;  /* [0, K_V] → [0, 1] */
    if (V_norm > 1.0f) V_norm = 1.0f;

    float SOM_norm = cell->SOM_percent / 5.0f;  /* [0, 5%] → [0, 1] (assume 5% is "excellent") */
    if (SOM_norm > 1.0f) SOM_norm = 1.0f;

    float theta_norm = cell->theta / cell->theta_s;  /* [0, θ_s] → [0, 1] */
    if (theta_norm > 1.0f) theta_norm = 1.0f;

    /* Weighted sum */
    float score = w_V * V_norm + w_SOM * SOM_norm + w_theta * theta_norm;

    return score;
}
