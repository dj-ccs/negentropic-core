# Core Physics Specification v0.3.3

**Version:** v0.3.3
**Status:** LOCKED – Production Ready
**Last Updated:** 2025-11-16

## Purpose

This document details the mathematical implementation of the core physics solvers: HYD-RLv1 (Richards-Lite Hydrology), REGv1/v2 (Regeneration Cascade + Microbial Priming), SE(3) tile-local frames on cubed-sphere projection, and D8/D∞ flow routing algorithms.

---

## 0. Implementation Status Summary (from Architecture Quick Reference)

| Component | Status | Tests | Performance |
|-----------|--------|-------|-------------|
| HYD-RLv1 | ✅ Production | TBD | <20 ns/cell ✅ |
| REGv1 | ✅ Production | 36/38 | 20-30 ns/cell ✅ |
| REGv2 | ✅ Production | 44/49 | 40-48 cycles/cell ✅ |
| SE(3) Transforms | ✅ Production | 15/15 | 15-18 ns/op ✅ |

---

## 1. HYD-RLv1: Richards-Lite Hydrology Solver

### 1.1 Governing Equation

**Unified Richards Equation:**
`∂θ/∂t = ∇·[K_eff(θ,I,ζ) ∇(ψ+z)] + S_I(x,y,t)`

Where:
- `θ`: Volumetric soil moisture (m³/m³)
- `K_eff`: Effective hydraulic conductivity (m/s)
- `ψ`: Matric potential (m)
- `z`: Elevation (m)
- `I`: Intervention multiplier
- `ζ`: Microtopography fill-and-spill parameter
- `S_I`: Infiltration source term (mm/hr → m/s)

This formulation handles both Hortonian (infiltration-excess) and Dunne (saturation-excess) runoff within a single PDE.

### 1.2 Discrete Implementation

The solver uses operator splitting for stability and performance:
1. **Vertical (implicit):** Solves the 1D Richards equation for each soil column.
2. **Horizontal (explicit):** Routes surface and subsurface flow between columns.

### 1.3 Fill-and-Spill Microtopography

A sigmoid capacity function, `C(ζ) = 1 / (1 + exp[-a_c(ζ - ζ_c)])`, modulates effective conductivity to simulate the filling of local depressions before overland flow begins.

### 1.4 Intervention Multipliers

User interventions directly modify hydraulic parameters in the simulation:
- **Gravel Mulch:** `K_eff *= 6.0f;` (Empirical value from Li et al. 2003, Loess Plateau).
- **Swales:** Increase depression storage capacity, reducing runoff by 50-70% until filled.
- **Check Dams:** Provide 100% water retention up to their capacity, directly recharging local soil moisture.

### 1.5 Van Genuchten Parameterization (Performance: <20 ns/cell with ~13x speedup)

The solver uses 256-entry Lookup Tables (LUTs) for the Van Genuchten soil hydraulic properties, providing a ~13x speedup over runtime `pow()` calculations.

**Hydraulic Conductivity:**
`K(S_e) = K_sat · √S_e · [1 - (1 - S_e^(1/m))^m]²`

**Matric Potential:**
`ψ(S_e) = -1/α · (S_e^(-1/m) - 1)^(1/n)`

**Canonical LUT Initialization (`init_van_genuchten_lut`):**
```c
#define VG_LUT_SIZE 256
float K_lut[VG_LUT_SIZE];
float psi_lut[VG_LUT_SIZE];

void init_van_genuchten_lut(float K_sat, float alpha, float n, float theta_s, float theta_r) {
    float m = 1.0f - 1.0f / n;
    for (int i = 0; i < VG_LUT_SIZE; i++) {
        float S_e = (float)i / (VG_LUT_SIZE - 1);
        // Calculate K(S_e) and ψ(S_e) and store in K_lut[i] and psi_lut[i]
        // ...
    }
}
```

### 1.6 D8 Flow Routing

The default routing algorithm uses a steepest-descent, single-direction flow model. Flow direction is determined by the maximum slope to one of 8 neighbors, considering both surface elevation `z` and water depth `h`: `Slope = ( (z_i + h_i) - (z_j + h_j) ) / L_ij`.

### 1.7 D∞ Flow Routing

An optional, higher-fidelity model that distributes flow to two downstream neighbors based on the steepest triangular facet, resulting in smoother, more realistic flow patterns at a ~30% performance cost.

---

## 2. REGv1: Regeneration Cascade Solver

### 2.1 Governing Equations

This solver models the slow-timescale (yearly) positive feedback loop between vegetation, soil organic matter (SOM), and moisture.

**Vegetation Growth:**
`dV/dt = r_V · V · (1 - V/K_V) + λ1 · max(θ - θ*, 0) + λ2 · max(SOM - SOM*, 0)`

**Soil Organic Matter:**
`dSOM/dt = a1 · V - a2 · SOM`

### 2.2 Hydrological Feedback

Changes in SOM from the REGv1 solver directly feed back into the HYD-RLv1 solver's parameters.

**Porosity Enhancement:** `Φ_eff = Φ_base + η1 · dSOM` (+1% SOM → +5mm water holding capacity)
**Conductivity Enhancement:** `K_zz *= (1.15)^dSOM` (+1% SOM → 15% conductivity increase)

### 2.3 Discrete Time Integration

A simple explicit Euler method is used with a yearly timestep, called approximately 68 times per simulation year (every 128 hydrology steps).

---

## 3. REGv2: Microbial Priming & Condenser Landscapes

This solver implements the microscale biological and atmospheric-interface dynamics that drive explosive, nonlinear regeneration. **Validation:** 44/49 tests (90%) passing, including the Johnson-Su 50× SOM explosion scenario.

### 3.1 Fungal-to-Bacterial Ratio (F:B) Priming

An 8-entry LUT maps the F:B ratio to a SOM production multiplier, anchored by real-world data (e.g., F:B > 10.0 yields an 8.0x multiplier, up to 10.0x for Johnson-Su compost conditions).

### 3.2 Aggregation-Conductivity Coupling

Hydraulic conductivity is dynamically enhanced based on soil aggregation and fungal hyphae presence:
`K(θ) = K₀ · [1 + m·Φ_agg·S(Φ_agg)] · [1 + α_myco·Φ_hyphae] · R(θ)`

### 3.3 Condensation Flux

Models non-rainfall water inputs (dew/fog), with a significant bonus for interventions like rock mulches (`β_rock = 50x` enhancement).
`F_cond = C_base · (1 + β_rock · I_rock) · ΔT_sat`

### 3.4 Hydraulic Lift

Models nocturnal water redistribution from deep soil layers to the surface via plant roots, gated by time of day.
`F_lift = k_lift · (θ_deep - θ_surface) · V · night_gate`

---

## 4. SE(3) Tile-Local Frames

The engine uses a **cubed-sphere projection** to avoid polar singularities and provide seamless global coverage. Physics are computed in tile-local frames using **quaternion-based SE(3) transforms**. **Performance:** <20 ns/transform, 16-byte aligned.

### 4.1 Cubed-Sphere Coordinate System

The planetary surface is mapped to the six faces of a cube, providing a set of locally-Euclidean coordinate systems. Positions are referenced by `(face, u, v)`.

### 4.2 Transform Functions

Core functions transform coordinates between the global Earth-Centered, Earth-Fixed (ECEF) frame and the local frame of any given tile on the cubed sphere.

### 4.3 Quaternion Operations

All 3D rotations are handled using quaternions to avoid gimbal lock and for efficient composition.
**Rotate Vector (`q * v * q⁻¹`):**
```c
void se3_rotate_vector(const float q[4], const float v[3], float out[3]) {
    // Optimized formula: v' = v + 2w(qv × v) + 2(qv × (qv × v))
    // ...
}
```
**Quaternion Multiplication (composition):**
```c
void se3_quat_multiply(const float q1[4], const float q2[4], float out[4]) {
    // ...
}
```

### 4.5 Intervention Application Protocol

When a user places an intervention, the core calls a dedicated function to directly modify cell hydraulic parameters.
```c
void apply_intervention(Cell* cell, InterventionType type) {
    switch (type) {
        case GRAVEL_MULCH:  cell->K_sat *= 6.0f; break;
        case SWALE:         cell->depression_storage += 0.5f; break; // +500 mm
        case CHECK_DAM:     cell->retention_capacity += 1.0f; break;
        case TREE_PLANTING: cell->vegetation_cover += 0.15f; break;
    }
    // Re-initialize affected Van Genuchten LUTs
    init_van_genuchten_lut_for_cell(cell);
}
```

---

## 5. Performance Benchmarks

| Solver | Target | Current | Status |
|--------|--------|---------|--------|
| HYD-RLv1 (with LUTs) | <20 ns/cell | 15-18 ns/cell | ✅ |
| REGv1 | <30 ns/cell | 20-30 ns/cell | ✅ |
| REGv2 | <50 cycles/cell | 40-48 cycles/cell | ✅ |
| D8 routing | <10 ns/cell | 8-10 ns/cell | ✅ |
| D∞ routing | <15 ns/cell | 12-14 ns/cell | ✅ |
| SE(3) transform | <20 ns/op | 15-18 ns/op | ✅ |

*Platform: x86-64, GCC 11.4, -O3 -march=native*

---

## 6. Validation Status

| Component | Tests Passing | Validation Source |
|-----------|---------------|-------------------|
| HYD-RLv1 | TBD | Weill et al. (2009) |
| REGv1 | 36/38 (94.7%) | Loess Plateau 1995-2010 |
| REGv2 | 44/49 (90%) | Johnson-Su compost, Makarieva |
| D8 routing | 10/10 (100%) | Synthetic DEMs |
| D∞ routing | 8/10 (80%) | Synthetic DEMs |
| SE(3) math | 15/15 (100%) | Unit tests |

---

## 7. References

1. **Richards Equation:** Weill et al., "A generalized Richards equation for surface/subsurface flow modeling" (2009)
2. **Fill-and-Spill:** Frei et al., "Surface water fill-and-spill in low-gradient landscapes" (2010)
3. **Loess Plateau:** Li et al., "Effect of gravel mulch on soil moisture and vegetation dynamics" (2003)
4. **Van Genuchten:** Van Genuchten, "A closed-form equation for predicting the hydraulic conductivity of unsaturated soils" (1980)
5. **D8/D∞ Routing:** Tarboton, "A new method for the determination of flow directions" (1997)
6. **Cubed-Sphere:** Ronchi et al., "The 'Cubed-Sphere': A new method for the solution of PDEs on the sphere" (1996)
7. **Quaternions:** Shoemake, "Quaternion calculus and fast animation" (1985)

---

## Document Control

- **Approved by:** Technical Lead
- **Review Frequency:** Every major release
- **Next Review:** v0.4.0
