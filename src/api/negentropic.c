/*
 * negentropic.c - Public C API Implementation
 *
 * Safe, minimal implementation with proper error handling.
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#include "negentropic.h"
#include "../core/state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple JSON parsing (for now, just parse basic config) */
/* TODO: Replace with proper JSON parser (cJSON, etc.) */

/* Thread-local error storage */
static __thread char last_error[256] = {0};

static void set_error(const char* msg) {
    strncpy(last_error, msg, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
}

/* ========================================================================
 * VERSION INFORMATION
 * ======================================================================== */

const char* neg_get_version(void) {
    return "0.1.0";
}

/* ========================================================================
 * BASIC JSON PARSING (Stub)
 * ======================================================================== */

static bool parse_config_json(const char* json, SimulationConfig* cfg) {
    if (!json || !cfg) return false;

    /* TODO: Replace with proper JSON parser */
    /* For now, set sensible defaults */
    memset(cfg, 0, sizeof(SimulationConfig));

    cfg->num_entities = 100;
    cfg->num_scalar_fields = 10000;
    cfg->grid_width = 100;
    cfg->grid_height = 100;
    cfg->grid_depth = 1;
    cfg->dt = 0.016f;  /* ~60 FPS */
    cfg->precision_mode = 1;  /* FP32 */
    cfg->integrator_type = 0;  /* Lie-Euler */
    cfg->enable_atmosphere = 1;
    cfg->enable_hydrology = 0;
    cfg->enable_soil = 1;

    /* Very basic parsing - look for key values */
    const char* p;

    if ((p = strstr(json, "\"num_entities\":"))) {
        sscanf(p + 16, "%u", &cfg->num_entities);
    }
    if ((p = strstr(json, "\"num_scalar_fields\":"))) {
        sscanf(p + 20, "%u", &cfg->num_scalar_fields);
    }
    if ((p = strstr(json, "\"grid_width\":"))) {
        sscanf(p + 13, "%u", &cfg->grid_width);
    }
    if ((p = strstr(json, "\"grid_height\":"))) {
        sscanf(p + 14, "%u", &cfg->grid_height);
    }
    if ((p = strstr(json, "\"grid_depth\":"))) {
        sscanf(p + 13, "%u", &cfg->grid_depth);
    }
    if ((p = strstr(json, "\"dt\":"))) {
        sscanf(p + 5, "%f", &cfg->dt);
    }
    if ((p = strstr(json, "\"precision_mode\":"))) {
        sscanf(p + 17, "%hhu", &cfg->precision_mode);
    }
    if ((p = strstr(json, "\"integrator_type\":"))) {
        sscanf(p + 18, "%hhu", &cfg->integrator_type);
    }

    return true;
}

/* ========================================================================
 * LIFECYCLE MANAGEMENT
 * ======================================================================== */

void* neg_create(const char* config_json) {
    if (!config_json) {
        set_error("NULL config_json");
        return NULL;
    }

    SimulationConfig cfg;
    if (!parse_config_json(config_json, &cfg)) {
        set_error("Failed to parse config JSON");
        return NULL;
    }

    void* sim = state_create(&cfg);
    if (!sim) {
        set_error("Failed to create simulation state");
        return NULL;
    }

    return sim;
}

void neg_destroy(void* sim) {
    state_destroy(sim);
}

/* ========================================================================
 * SIMULATION CONTROL
 * ======================================================================== */

int neg_step(void* sim, float dt) {
    if (!sim) {
        set_error("NULL simulation handle");
        return NEG_ERROR_NULL_HANDLE;
    }

    if (!state_step(sim, dt)) {
        set_error("Numerical instability detected");
        return NEG_ERROR_NUMERICAL_INSTABILITY;
    }

    return NEG_SUCCESS;
}

int neg_reset_from_binary(void* sim, const uint8_t* buffer, size_t len) {
    if (!sim) {
        set_error("NULL simulation handle");
        return NEG_ERROR_NULL_HANDLE;
    }

    if (!buffer || len == 0) {
        set_error("Invalid buffer");
        return NEG_ERROR_INVALID_STATE;
    }

    if (!state_reset_from_binary(sim, buffer, len)) {
        set_error("Failed to reset from binary state");
        return NEG_ERROR_INVALID_STATE;
    }

    return NEG_SUCCESS;
}

/* ========================================================================
 * STATE RETRIEVAL
 * ======================================================================== */

int neg_get_state_binary(void* sim, uint8_t* buffer, size_t max_len) {
    if (!sim) {
        set_error("NULL simulation handle");
        return NEG_ERROR_NULL_HANDLE;
    }

    if (!buffer) {
        set_error("NULL buffer");
        return NEG_ERROR_INVALID_STATE;
    }

    size_t required = state_get_binary_size(sim);
    if (max_len < required) {
        set_error("Buffer too small");
        return NEG_ERROR_BUFFER_TOO_SMALL;
    }

    size_t written = state_to_binary(sim, buffer, max_len);
    if (written == 0) {
        set_error("Failed to serialize state");
        return NEG_ERROR_INVALID_STATE;
    }

    return (int)written;
}

size_t neg_get_state_binary_size(void* sim) {
    if (!sim) return 0;
    return state_get_binary_size(sim);
}

int neg_get_state_json(void* sim, char* buffer, size_t max_len) {
    if (!sim) {
        set_error("NULL simulation handle");
        return NEG_ERROR_NULL_HANDLE;
    }

    if (!buffer || max_len == 0) {
        set_error("Invalid buffer");
        return NEG_ERROR_INVALID_STATE;
    }

    SimulationState state;
    if (!state_get_view(sim, &state)) {
        set_error("Failed to get state view");
        return NEG_ERROR_INVALID_STATE;
    }

    /* TODO: Proper JSON serialization */
    /* For now, simple format */
    int written = snprintf(buffer, max_len,
        "{\"timestamp\":%llu,\"version\":%u,\"num_entities\":%u,\"num_scalar_values\":%u,\"hash\":%llu}",
        (unsigned long long)state.timestamp,
        state.version,
        state.num_entities,
        state.num_scalar_values,
        (unsigned long long)state.state_hash
    );

    if (written < 0 || (size_t)written >= max_len) {
        set_error("Buffer too small for JSON");
        return NEG_ERROR_BUFFER_TOO_SMALL;
    }

    return written;
}

/* ========================================================================
 * VALIDATION & DIAGNOSTICS
 * ======================================================================== */

uint64_t neg_get_state_hash(void* sim) {
    if (!sim) return 0;
    return state_hash(sim);
}

const char* neg_get_last_error(void) {
    return last_error[0] ? last_error : NULL;
}

int neg_get_diagnostics(void* sim, char* buffer, size_t max_len) {
    if (!sim) {
        set_error("NULL simulation handle");
        return NEG_ERROR_NULL_HANDLE;
    }

    if (!buffer || max_len == 0) {
        set_error("Invalid buffer");
        return NEG_ERROR_INVALID_STATE;
    }

    SimulationState state;
    if (!state_get_view(sim, &state)) {
        set_error("Failed to get state view");
        return NEG_ERROR_INVALID_STATE;
    }

    /* TODO: More detailed diagnostics */
    int written = snprintf(buffer, max_len,
        "{\"energy\":%.6f,\"max_error\":%.9f,\"timestamp\":%llu}",
        state.energy,
        state.max_error,
        (unsigned long long)state.timestamp
    );

    if (written < 0 || (size_t)written >= max_len) {
        set_error("Buffer too small for diagnostics");
        return NEG_ERROR_BUFFER_TOO_SMALL;
    }

    return written;
}
