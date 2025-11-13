/*
 * state.c - Canonical Simulation State Implementation
 *
 * Memory layout: Single contiguous block
 *   [SimulationInternal][poses array][scalar_fields array]
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#include "state.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* XXH3 for deterministic hashing (stub for now) */
#define XXH_INLINE_ALL
/* TODO: Include xxhash library */
static uint64_t xxh3_hash(const void* data, size_t len) {
    /* Simple FNV-1a hash as placeholder */
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * Internal simulation state (opaque to external users).
 *
 * Single contiguous memory block layout:
 *   [SimulationInternal][poses][scalar_fields]
 */
typedef struct {
    SimulationConfig config;        /* Configuration snapshot */
    uint64_t timestamp;             /* Current simulation time (microseconds) */
    uint64_t step_count;            /* Number of steps executed */

    /* Memory block offsets */
    size_t poses_offset;            /* Offset to poses array */
    size_t scalar_fields_offset;    /* Offset to scalar_fields array */

    /* Diagnostics */
    float total_energy;             /* System energy */
    float max_numerical_error;      /* Max error in last step */

    /* Error tracking */
    char last_error[256];           /* Last error message */

    /* Data follows this struct in memory:
     *   se3_pose_t poses[config.num_entities];
     *   float scalar_fields[config.num_scalar_fields];
     */
} SimulationInternal;

/* ========================================================================
 * STATE CREATION & DESTRUCTION
 * ======================================================================== */

void* state_create(const SimulationConfig* cfg) {
    if (!cfg) return NULL;
    if (cfg->num_entities == 0) return NULL;

    /* Calculate memory layout */
    size_t base_size = sizeof(SimulationInternal);
    size_t poses_size = cfg->num_entities * sizeof(se3_pose_t);
    size_t scalar_fields_size = cfg->num_scalar_fields * sizeof(float);
    size_t total_size = base_size + poses_size + scalar_fields_size;

    /* Allocate single contiguous block */
    void* memory = calloc(1, total_size);
    if (!memory) return NULL;

    SimulationInternal* sim = (SimulationInternal*)memory;

    /* Initialize configuration */
    sim->config = *cfg;
    sim->timestamp = 0;
    sim->step_count = 0;
    sim->total_energy = 0.0f;
    sim->max_numerical_error = 0.0f;
    sim->last_error[0] = '\0';

    /* Set memory offsets */
    sim->poses_offset = base_size;
    sim->scalar_fields_offset = base_size + poses_size;

    /* Initialize poses to identity */
    se3_pose_t* poses = (se3_pose_t*)((uint8_t*)memory + sim->poses_offset);
    for (uint32_t i = 0; i < cfg->num_entities; i++) {
        se3_pose_identity(&poses[i]);
    }

    /* Initialize scalar fields to zero (already done by calloc) */

    return (void*)sim;
}

void state_destroy(void* sim) {
    if (sim) {
        free(sim);
    }
}

/* ========================================================================
 * STATE ACCESS
 * ======================================================================== */

bool state_get_view(void* sim, SimulationState* out_state) {
    if (!sim || !out_state) return false;

    SimulationInternal* internal = (SimulationInternal*)sim;
    uint8_t* memory = (uint8_t*)sim;

    out_state->timestamp = internal->timestamp;
    out_state->version = 1;  /* Schema version */
    out_state->num_entities = internal->config.num_entities;
    out_state->num_scalar_values = internal->config.num_scalar_fields;

    /* Set pointers into memory block */
    out_state->poses = (se3_pose_t*)(memory + internal->poses_offset);
    out_state->scalar_fields = (float*)(memory + internal->scalar_fields_offset);

    out_state->precision_mode = internal->config.precision_mode;
    out_state->state_hash = state_hash(sim);
    out_state->energy = internal->total_energy;
    out_state->max_error = internal->max_numerical_error;

    return true;
}

/* ========================================================================
 * SIMULATION STEPPING (Stub - to be implemented with integrators)
 * ======================================================================== */

bool state_step(void* sim, float dt) {
    if (!sim) return false;

    SimulationInternal* internal = (SimulationInternal*)sim;

    /* Use config default if dt == 0 */
    if (dt == 0.0f) {
        dt = internal->config.dt;
    }

    /* Get data pointers */
    uint8_t* memory = (uint8_t*)sim;
    se3_pose_t* poses = (se3_pose_t*)(memory + internal->poses_offset);
    float* scalar_fields = (float*)(memory + internal->scalar_fields_offset);

    /* TODO: Call integrator based on config.integrator_type
     * For now, this is a stub that does nothing
     */

    /* Update timestamp */
    internal->timestamp += (uint64_t)(dt * 1e6);  /* Convert to microseconds */
    internal->step_count++;

    /* Stub: No actual physics yet */
    internal->max_numerical_error = 0.0f;

    return true;
}

/* ========================================================================
 * STATE SERIALIZATION
 * ======================================================================== */

size_t state_get_binary_size(void* sim) {
    if (!sim) return 0;

    SimulationInternal* internal = (SimulationInternal*)sim;

    /* Binary format: version + timestamp + num_entities + poses + num_scalars + scalar_fields */
    size_t size = 0;
    size += sizeof(uint32_t);  /* version */
    size += sizeof(uint64_t);  /* timestamp */
    size += sizeof(uint32_t);  /* num_entities */
    size += internal->config.num_entities * sizeof(se3_pose_t);
    size += sizeof(uint32_t);  /* num_scalar_fields */
    size += internal->config.num_scalar_fields * sizeof(float);

    return size;
}

size_t state_to_binary(void* sim, uint8_t* buffer, size_t max_len) {
    if (!sim || !buffer) return 0;

    size_t required_size = state_get_binary_size(sim);
    if (max_len < required_size) return 0;

    SimulationInternal* internal = (SimulationInternal*)sim;
    uint8_t* memory = (uint8_t*)sim;
    se3_pose_t* poses = (se3_pose_t*)(memory + internal->poses_offset);
    float* scalar_fields = (float*)(memory + internal->scalar_fields_offset);

    uint8_t* ptr = buffer;

    /* Write version */
    uint32_t version = 1;
    memcpy(ptr, &version, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    /* Write timestamp */
    memcpy(ptr, &internal->timestamp, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    /* Write poses */
    uint32_t num_entities = internal->config.num_entities;
    memcpy(ptr, &num_entities, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, poses, num_entities * sizeof(se3_pose_t));
    ptr += num_entities * sizeof(se3_pose_t);

    /* Write scalar fields */
    uint32_t num_scalar_fields = internal->config.num_scalar_fields;
    memcpy(ptr, &num_scalar_fields, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, scalar_fields, num_scalar_fields * sizeof(float));
    ptr += num_scalar_fields * sizeof(float);

    return (size_t)(ptr - buffer);
}

bool state_reset_from_binary(void* sim, const uint8_t* buffer, size_t len) {
    if (!sim || !buffer || len == 0) return false;

    SimulationInternal* internal = (SimulationInternal*)sim;
    uint8_t* memory = (uint8_t*)sim;

    const uint8_t* ptr = buffer;
    const uint8_t* end = buffer + len;

    /* Read version */
    if (ptr + sizeof(uint32_t) > end) return false;
    uint32_t version;
    memcpy(&version, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (version != 1) return false;  /* Unsupported version */

    /* Read timestamp */
    if (ptr + sizeof(uint64_t) > end) return false;
    memcpy(&internal->timestamp, ptr, sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    /* Read poses */
    if (ptr + sizeof(uint32_t) > end) return false;
    uint32_t num_entities;
    memcpy(&num_entities, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (num_entities != internal->config.num_entities) return false;

    size_t poses_size = num_entities * sizeof(se3_pose_t);
    if (ptr + poses_size > end) return false;

    se3_pose_t* poses = (se3_pose_t*)(memory + internal->poses_offset);
    memcpy(poses, ptr, poses_size);
    ptr += poses_size;

    /* Read scalar fields */
    if (ptr + sizeof(uint32_t) > end) return false;
    uint32_t num_scalar_fields;
    memcpy(&num_scalar_fields, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (num_scalar_fields != internal->config.num_scalar_fields) return false;

    size_t scalar_fields_size = num_scalar_fields * sizeof(float);
    if (ptr + scalar_fields_size > end) return false;

    float* scalar_fields = (float*)(memory + internal->scalar_fields_offset);
    memcpy(scalar_fields, ptr, scalar_fields_size);
    ptr += scalar_fields_size;

    return true;
}

/* ========================================================================
 * HASHING
 * ======================================================================== */

uint64_t state_hash(void* sim) {
    if (!sim) return 0;

    /* Hash the binary representation for determinism */
    size_t size = state_get_binary_size(sim);
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) return 0;

    size_t written = state_to_binary(sim, buffer, size);
    if (written == 0) {
        free(buffer);
        return 0;
    }

    uint64_t hash = xxh3_hash(buffer, written);
    free(buffer);

    return hash;
}
