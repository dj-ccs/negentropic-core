# LoD and Cascading Simulation System v0.3.3

**Version:** v0.3.3
**Status:** ✅ Temporal Cascade, 🚧 Spatial LoD (planned)
**Last Updated:** 2025-11-16

## Purpose

This document specifies the complete mechanics of the multi-scale simulation engine in `negentropic-core`. It defines the conservative flux-transfer formulas for upscaling and downscaling (moisture, momentum, runoff), rules for handling nonlinearities, and the camera-driven hysteresis protocol for LoD grid refinement and coarsening.

---

## 0. Current Status (from Architecture Quick Reference)

| Item | Status | Notes |
|------|--------|-------|
| Temporal Cascade (128× REG) | ✅ Production | 99.2% call reduction, accumulation buffers |
| Spatial LoD (4 levels) | 🚧 Planned | Quad-tree, camera + importance driven |
| Conservative Flux Transfer | Spec complete | Intensive avg, extensive sum, K(θ) avg |
| Hysteresis State Machine | Spec complete | Prevents flickering |
| Smooth Transitions | Spec complete | 30-frame blend (3 s @ 10 Hz) |
| Performance | ~10× cell reduction, <5% overhead (target) | TBD |

---

## 1. Overview

### 1.1 Multi-Scale Challenge

Regenerative ecosystem dynamics span 6+ orders of magnitude in time and space. To manage this complexity, a cascading simulation architecture is required.
- **Temporal cascade:** Fast physics (hydrology) runs more frequently than slow physics (regeneration).
- **Spatial LoD:** The simulation grid is adaptively refined based on camera position and feature importance.

### 1.2 Design Philosophy

The cascading system is designed to be:
- **Conservative:** Total mass and energy are preserved across scale transitions.
- **Stable:** Hysteresis prevents rapid LoD flickering.
- **Efficient:** ~99% reduction in expensive solver calls, ~10× reduction in active cells.

---

## 2. Temporal Cascading

### 2.1 Timescale Hierarchy

| Solver | Timestep | Call Frequency | Rationale |
|--------|----------|----------------|-----------|
| HYD-RLv1 | 1 hour | Every frame (10 Hz) | Fast infiltration dynamics |
| REGv1/v2 | ~5.3 days | Every 128 HYD steps | Slow vegetation/microbial dynamics |
| ATMv1 | 10 seconds | Every frame | Atmospheric advection |
| Cloud Particles | 1 second | Every frame | Lagrangian advection |

This hierarchy yields a **~99.2% reduction** in calls to the computationally expensive REG solvers.

**Mathematical Justification:**
- HYD-RLv1 timescale: `τ_hyd ~ L²/K ~ (10 m)² / (10⁻⁵ m/s) ~ 3 hours`
- REGv1 timescale: `τ_reg ~ 1/r_V ~ 1/(0.05 yr⁻¹) ~ 20 years`
- The ratio `τ_reg / τ_hyd ~ 10⁴` justifies infrequent coupling.

### 2.2 Implementation

A master timestepping loop uses a counter to trigger solvers at their specified frequencies.

**Canonical Timestep Controller:**
```c
typedef struct {
    uint64_t tick;              // Global simulation tick counter
    uint64_t hyd_tick;          // Hydrology sub-tick
    uint64_t reg_tick;          // Regeneration sub-tick
    uint32_t hyd_steps_per_reg; // 128
} CascadeController;

void cascade_step(CascadeController* ctrl, SimulationState* state) {
    // Always run fast solvers
    hyd_step(state->hydrology, DT_HYD);
    atm_step(state->atmosphere, DT_ATM);
    cloud_step(state->clouds, DT_CLOUD);

    ctrl->hyd_tick++;
    ctrl->tick++;

    // Accumulate fast solver outputs for slow solvers
    accumulate_hyd_outputs(state->accumulators, state->hydrology);

    // Run slow solver every 128 HYD steps
    if (ctrl->hyd_tick % ctrl->hyd_steps_per_reg == 0) {
        // Compute time-averaged inputs from accumulation buffers
        float avg_theta = state->accumulators->theta_sum / 128.0f;
        float avg_precip = state->accumulators->precip_sum / 128.0f;

        // Run REG solver with averaged inputs
        reg_step(state->regeneration, avg_theta, avg_precip, DT_REG);

        // Reset accumulators
        reset_accumulators(state->accumulators);

        ctrl->reg_tick++;
    }
}
```

### 2.3 Accumulation Buffers

To provide time-averaged inputs to the slow solvers, outputs from the fast solvers (e.g., soil moisture, runoff) are summed in accumulation buffers over the slow timestep period (128 HYD steps).

**Canonical Accumulator Structure:**
```c
typedef struct {
    float* theta_sum;          // [num_cells] - Total moisture over 128 steps
    float* precip_sum;         // [num_cells] - Total precipitation
    float* runoff_sum;         // [num_cells] - Total runoff
    uint32_t num_samples;      // Current sample count (0-128)
} AccumulationBuffers;

void accumulate_hyd_outputs(AccumulationBuffers* acc, HydState* hyd) {
    for (uint32_t i = 0; i < hyd->num_cells; i++) {
        acc->theta_sum[i] += hyd->theta[i];
        acc->precip_sum[i] += hyd->precipitation[i];
        acc->runoff_sum[i] += hyd->runoff[i];
    }
    acc->num_samples++;
}

void reset_accumulators(AccumulationBuffers* acc) {
    memset(acc->theta_sum, 0, acc->num_cells * sizeof(float));
    memset(acc->precip_sum, 0, acc->num_cells * sizeof(float));
    memset(acc->runoff_sum, 0, acc->num_cells * sizeof(float));
    acc->num_samples = 0;
}
```

---

## 3. Spatial Level-of-Detail (LoD)

### 3.1 Quad-Tree Grid Hierarchy

The spatial domain is represented by a 4-level quad-tree on the cubed sphere:
- **Level 0:** 16×16 grid (100 km spacing)
- **Level 1:** 32×32 grid (50 km spacing)
- **Level 2:** 64×64 grid (25 km spacing)
- **Level 3:** 128×128 grid (12.5 km spacing)

**Quad-Tree Node Structure:**
```c
typedef struct QuadNode {
    uint8_t level;              // 0-3
    uint16_t x, y;              // Grid coordinates at this level
    SimulationCell data;        // Cell state (θ, SOM, V, etc.)
    struct QuadNode* children[4]; // NULL if leaf, else NW/NE/SW/SE
    struct QuadNode* parent;    // NULL if root
    float importance_cache;     // Cached importance metric
} QuadNode;
```

### 3.2 Camera-Driven Refinement

A grid cell is refined based on two criteria:
1. **Distance to camera:** `dist < 50 km`
2. **Feature importance:** `I(cell) > 0.5`, where `I(cell) = |∇θ| + |∇V| + |∇SOM| + α·runoff`

**Canonical Importance Metric:**
```c
float compute_importance(QuadNode* node, QuadNode* neighbors[8]) {
    float grad_theta = 0.0f;
    float grad_veg = 0.0f;
    float grad_som = 0.0f;

    // Compute gradients via central differences
    for (int i = 0; i < 8; i++) {
        if (neighbors[i] == NULL) continue;
        grad_theta += fabsf(node->data.theta - neighbors[i]->data.theta);
        grad_veg += fabsf(node->data.vegetation - neighbors[i]->data.vegetation);
        grad_som += fabsf(node->data.som - neighbors[i]->data.som);
    }

    // Normalize by number of neighbors
    int num_neighbors = 0;
    for (int i = 0; i < 8; i++) if (neighbors[i] != NULL) num_neighbors++;
    if (num_neighbors > 0) {
        grad_theta /= num_neighbors;
        grad_veg /= num_neighbors;
        grad_som /= num_neighbors;
    }

    // Combine gradients with runoff term
    return grad_theta + grad_veg + grad_som + ALPHA_RUNOFF * node->data.runoff;
}
```

### 3.3 Hysteresis Protocol

To prevent rapid flickering at LoD boundaries, separate and non-overlapping thresholds are used for refinement and coarsening, managed by a formal state machine for each grid region.
- **Refine:** `dist < 50 km` AND `importance > 0.5`
- **Coarsen:** `dist > 75 km` OR `importance < 0.3`

**Canonical State Machine:**
```c
typedef enum {
    LOD_STATE_COARSE,
    LOD_STATE_REFINING,
    LOD_STATE_FINE,
    LOD_STATE_COARSENING
} LoDState;

typedef struct {
    LoDState state;
    uint32_t transition_frame;  // Frame when transition started
    uint32_t frames_in_state;   // Frames since last state change
} LoDStateMachine;

void update_lod_state(QuadNode* node, vec3 camera_pos, uint32_t current_frame) {
    float dist = distance(node->data.position, camera_pos) / 1000.0f; // km
    float importance = node->importance_cache;

    LoDStateMachine* sm = &node->lod_state_machine;
    sm->frames_in_state++;

    switch (sm->state) {
        case LOD_STATE_COARSE:
            if (dist < 50.0f && importance > 0.5f) {
                sm->state = LOD_STATE_REFINING;
                sm->transition_frame = current_frame;
                sm->frames_in_state = 0;
                refine_node(node);
            }
            break;

        case LOD_STATE_REFINING:
            if (sm->frames_in_state >= BLEND_DURATION_FRAMES) {
                sm->state = LOD_STATE_FINE;
                sm->frames_in_state = 0;
            }
            break;

        case LOD_STATE_FINE:
            if (dist > 75.0f || importance < 0.3f) {
                sm->state = LOD_STATE_COARSENING;
                sm->transition_frame = current_frame;
                sm->frames_in_state = 0;
                coarsen_node(node);
            }
            break;

        case LOD_STATE_COARSENING:
            if (sm->frames_in_state >= BLEND_DURATION_FRAMES) {
                sm->state = LOD_STATE_COARSE;
                sm->frames_in_state = 0;
            }
            break;
    }
}
```

---

## 4. Conservative Flux Transfer

### 4.1 Upscaling (Fine → Coarse)

**Rule:** Average intensive quantities, sum extensive quantities.

**Intensive Quantities (temperature, moisture content):**
`θ_coarse = (1/4) · Σ θ_fine_child`

**Extensive Quantities (total water volume, runoff):**
`V_coarse = Σ V_fine_child`

**Canonical Upscaling Function:**
```c
void upscale_to_parent(QuadNode* parent) {
    // Reset parent state
    parent->data.theta = 0.0f;
    parent->data.vegetation = 0.0f;
    parent->data.som = 0.0f;
    parent->data.total_water_volume = 0.0f;
    parent->data.runoff = 0.0f;

    // Accumulate from 4 children
    for (int i = 0; i < 4; i++) {
        QuadNode* child = parent->children[i];
        if (child == NULL) continue;

        // Average intensive quantities
        parent->data.theta += child->data.theta / 4.0f;
        parent->data.vegetation += child->data.vegetation / 4.0f;
        parent->data.som += child->data.som / 4.0f;

        // Sum extensive quantities
        parent->data.total_water_volume += child->data.total_water_volume;
        parent->data.runoff += child->data.runoff;
    }
}
```

**CRITICAL: Nonlinearity Handling**

The result of a nonlinear function must be calculated at the fine scale before being aggregated.

**Example: Hydraulic Conductivity**
❌ **WRONG:** `K_coarse = K(avg(θ_fine))`
✅ **CORRECT:** `K_coarse = avg(K(θ_fine))`

```c
void upscale_hydraulic_conductivity(QuadNode* parent) {
    float K_sum = 0.0f;
    for (int i = 0; i < 4; i++) {
        QuadNode* child = parent->children[i];
        if (child == NULL) continue;

        // Compute K at fine scale FIRST
        float K_child = compute_hydraulic_conductivity(child->data.theta);
        K_sum += K_child;
    }
    parent->data.K_sat = K_sum / 4.0f;  // Average the computed values
}
```

### 4.2 Downscaling (Coarse → Fine)

**Smooth fields (e.g., temperature, moisture):** Use bilinear interpolation to initialize the new fine grid from the parent coarse grid.

**Extensive quantities (e.g., runoff):** Distribute the coarse cell's total quantity equally among its new child fine cells to conserve mass.

**Canonical Downscaling Function:**
```c
void downscale_to_children(QuadNode* parent) {
    for (int i = 0; i < 4; i++) {
        QuadNode* child = parent->children[i];
        if (child == NULL) continue;

        // Initialize intensive quantities via interpolation
        // (Simple copy for now, bilinear interpolation from neighbors for production)
        child->data.theta = parent->data.theta;
        child->data.temperature = parent->data.temperature;
        child->data.vegetation = parent->data.vegetation;
        child->data.som = parent->data.som;

        // Distribute extensive quantities equally
        child->data.total_water_volume = parent->data.total_water_volume / 4.0f;
        child->data.runoff = parent->data.runoff / 4.0f;
    }
}
```

---

## 5. Refinement Transition Protocol

### 5.1 Smooth Transition

To avoid visual and physical discontinuities, the state of a newly refined/coarsened grid is blended between its old and new states over a **30-frame transition period (3 seconds at 10 Hz)**.

**Canonical Blending Function:**
```c
void blend_lod_transition(QuadNode* node, uint32_t current_frame) {
    LoDStateMachine* sm = &node->lod_state_machine;

    if (sm->state != LOD_STATE_REFINING && sm->state != LOD_STATE_COARSENING) {
        return;  // No blending needed
    }

    uint32_t frames_since_transition = current_frame - sm->transition_frame;
    float blend_factor = (float)frames_since_transition / BLEND_DURATION_FRAMES;
    blend_factor = fminf(blend_factor, 1.0f);  // Clamp to [0, 1]

    // Blend between old (cached) and new state
    node->data.theta = lerp(node->data.theta_old, node->data.theta_new, blend_factor);
    node->data.vegetation = lerp(node->data.vegetation_old, node->data.vegetation_new, blend_factor);
    node->data.som = lerp(node->data.som_old, node->data.som_new, blend_factor);
}
```

### 5.2 Exact Locked Parameters (v0.3.3)

| Parameter | Value | Meaning |
|-----------|-------|---------|
| HYD → REG call frequency | 128 steps | ~5.3 days at 10 Hz |
| Refine distance threshold | < 50 km | |
| Coarsen distance threshold | > 75 km | |
| Feature importance α | 0.1 | Runoff weighting in importance |
| Blend duration | 30 frames | 3 seconds at 10 Hz |

These values are now locked and must not change without a schema version bump.

---

## 6. Coarsening Protocol

When a fine grid is coarsened, summary statistics (mean, variance, min, max) of its state are computed and stored in the parent coarse cell to preserve sub-grid heterogeneity information for future refinement.

**Canonical Summary Statistics:**
```c
typedef struct {
    float mean;
    float variance;
    float min;
    float max;
} SubGridStats;

void compute_subgrid_stats(QuadNode* parent) {
    SubGridStats theta_stats = {0};
    SubGridStats veg_stats = {0};

    // First pass: compute mean
    for (int i = 0; i < 4; i++) {
        QuadNode* child = parent->children[i];
        if (child == NULL) continue;

        theta_stats.mean += child->data.theta;
        veg_stats.mean += child->data.vegetation;

        // Track min/max
        theta_stats.min = fminf(theta_stats.min, child->data.theta);
        theta_stats.max = fmaxf(theta_stats.max, child->data.theta);
    }
    theta_stats.mean /= 4.0f;
    veg_stats.mean /= 4.0f;

    // Second pass: compute variance
    for (int i = 0; i < 4; i++) {
        QuadNode* child = parent->children[i];
        if (child == NULL) continue;

        float theta_diff = child->data.theta - theta_stats.mean;
        theta_stats.variance += theta_diff * theta_diff;

        float veg_diff = child->data.vegetation - veg_stats.mean;
        veg_stats.variance += veg_diff * veg_diff;
    }
    theta_stats.variance /= 4.0f;
    veg_stats.variance /= 4.0f;

    // Store in parent
    parent->subgrid.theta_stats = theta_stats;
    parent->subgrid.veg_stats = veg_stats;
}
```

---

## 7. Performance Characteristics

The LoD system provides a **~10× reduction** in the number of active simulation cells compared to a uniform high-resolution grid, with an estimated total overhead of **< 5% of the frame budget**.

**Performance Breakdown (Estimated):**
| Operation | Cost per frame | Notes |
|-----------|----------------|-------|
| Importance computation | ~100 µs | 8-neighbor gradient for 1000 cells |
| LoD state machine updates | ~50 µs | 1000 cells |
| Upscaling/Downscaling | ~200 µs | ~10 transitions per frame |
| Blending | ~100 µs | ~50 cells in transition |
| **Total LoD overhead** | **~450 µs** | **< 5% of 10 ms frame budget** |

---

## 8. Spatial Partitioning (T-BSP Trees)

A Time-Bucketed Binary Space Partitioning tree is planned for efficiently managing cell data and performing frustum culling to avoid simulating cells outside the camera's view.

**Planned T-BSP Node:**
```c
typedef struct TBSPNode {
    AABB bounds;                // Axis-aligned bounding box
    uint32_t num_cells;         // Number of cells in this node
    QuadNode** cells;           // Cells in this node
    struct TBSPNode* left;      // Left child (split plane)
    struct TBSPNode* right;     // Right child
} TBSPNode;

// Frustum culling
void cull_and_simulate(TBSPNode* node, Frustum* camera_frustum) {
    if (!frustum_intersects_aabb(camera_frustum, &node->bounds)) {
        return;  // Skip entire subtree
    }

    if (node->left == NULL && node->right == NULL) {
        // Leaf node: simulate all cells
        for (uint32_t i = 0; i < node->num_cells; i++) {
            simulate_cell(node->cells[i]);
        }
    } else {
        // Recurse
        if (node->left) cull_and_simulate(node->left, camera_frustum);
        if (node->right) cull_and_simulate(node->right, camera_frustum);
    }
}
```

---

## 9. Future Extensions

- **Wavelet-Based Compression:** Use Haar wavelets to compress coarse-level grid data for sparse storage.
- **GPU-Accelerated Upscaling/Downscaling:** Implement bilinear interpolation and aggregation in compute shaders.
- **Adaptive Timestep:** Dynamically adjust `dt` based on local CFL conditions.
- **Importance-Driven Temporal Refinement:** Run fast solvers more frequently in high-importance regions.

---

## 10. Validation

### 10.1 Mass Conservation Test

The total mass/energy of a field must be identical (within numerical precision) before and after a full refine-coarsen cycle.

**Canonical Test:**
```c
void test_mass_conservation() {
    QuadNode* root = create_test_grid();

    // Compute total mass before refinement
    float total_mass_before = compute_total_mass(root);

    // Refine all cells
    refine_all_cells(root);

    // Compute total mass after refinement
    float total_mass_after_refine = compute_total_mass(root);

    // Coarsen all cells back
    coarsen_all_cells(root);

    // Compute total mass after coarsening
    float total_mass_after_coarsen = compute_total_mass(root);

    // Assert conservation
    assert(fabsf(total_mass_before - total_mass_after_refine) < 1e-6);
    assert(fabsf(total_mass_before - total_mass_after_coarsen) < 1e-6);
}
```

### 10.2 Hysteresis Test

A camera oscillating near an LoD boundary must not trigger rapid, repeated state changes.

**Canonical Test:**
```c
void test_hysteresis() {
    QuadNode* node = create_test_node();
    vec3 camera_pos = {55000.0f, 0.0f, 0.0f};  // 55 km (between thresholds)

    uint32_t num_state_changes = 0;
    LoDState prev_state = node->lod_state_machine.state;

    // Oscillate camera for 100 frames
    for (uint32_t frame = 0; frame < 100; frame++) {
        // Oscillate camera by ±10 km
        camera_pos.x = 55000.0f + 10000.0f * sinf(frame * 0.1f);

        update_lod_state(node, camera_pos, frame);

        if (node->lod_state_machine.state != prev_state) {
            num_state_changes++;
            prev_state = node->lod_state_machine.state;
        }
    }

    // Assert: hysteresis should prevent excessive state changes
    assert(num_state_changes < 5);  // Maximum 5 changes in 100 frames
}
```

---

## 11. References

1. **Temporal Operator Splitting:** Yanenko, N.N. "The Method of Fractional Steps" (1971)
2. **Conservative Remapping:** Jones, P.W. "Conservative remapping between unstructured grids" (1999)
3. **Quad-Tree LoD:** Losasso et al., "Simulating water and smoke with an octree data structure" (2004)
4. **T-BSP Trees:** Samet, H. "The Design and Analysis of Spatial Data Structures" (1990)
5. **Wavelet Compression:** Stollnitz et al., "Wavelets for Computer Graphics" (1995)
6. **Hysteresis in LoD:** Hoppe, H. "Smooth View-Dependent Level-of-Detail Control" (1998)

---

## Document Control

- **Approved by:** Technical Lead
- **Review Frequency:** Every major release
- **Next Review:** v0.4.0
