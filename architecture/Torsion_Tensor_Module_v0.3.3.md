# Torsion Tensor Module v0.3.3

**Version:** v0.3.3
**Status:** 🚧 Planned (awaiting ATMv1 completion)
**Last Updated:** 2025-11-16

## Purpose

This document specifies the complete physics and implementation of the "2.5D" vorticity simulation module. It defines the discrete curl operator for cubed-sphere grids, computation of vertical pseudo-velocity (`w_c`), and feedback mechanisms into buoyancy and convection terms in the atmospheric solver.

---

## 0. Current Status (from Architecture Quick Reference)

| Item | Status | Notes |
|------|--------|-------|
| Discrete Curl (cubed-sphere) | Spec complete | Edge transformation logic defined |
| w_c = -H · ∇²ω_z | Spec complete | H = 1000 m, 5-point stencil |
| Buoyancy feedback (α=0.1) | Spec complete | |
| Vortex force (ε=0.05) | Spec complete | |
| Temperature mixing (β=0.02) | Spec complete | |
| Full implementation | 🚧 Planned | Awaiting ATMv1 completion |
| Performance | ~50 cycles/cell (35% overhead) | Estimated |
| Validation | Synthetic test cases defined | N/A yet |

---

## 1. Overview

### 1.1 Motivation

**Problem:** Traditional 2D atmospheric models cannot capture rotational dynamics (vorticity), which are critical for accurate precipitation patterns, energy dissipation in turbulent flows, and realistic cloud formation.

**Solution: "2.5D" Torsion Tensor Module**
- Horizontal vorticity field (`ω_z`) computed from horizontal velocity.
- Vertical pseudo-velocity (`w_c`) inferred from vorticity (no full 3D solve).
- Feedback into the atmospheric solver's buoyancy and convection terms.

### 1.2 Physical Basis

**Vorticity (ω):** The curl of the velocity field, `ω = ∇ × u`. For 2D horizontal flow `u = (u_x, u_y, 0)`, this simplifies to `ω_z = ∂u_y/∂x - ∂u_x/∂y`.

**Vertical Pseudo-Velocity (w_c):** Approximated based on vorticity stretching, `w_c ≈ -H · ∇²ω_z`, where `H` is the characteristic atmospheric height (~1 km).

---

## 2. Discrete Curl Operator for Cubed-Sphere Grid

### 2.1 Grid Structure

The simulation uses a cubed-sphere topology with six faces, each having an NxN grid. Velocity is represented in local `(u_u, u_v)` coordinates on each face.

### 2.2 Finite Difference Curl

**`ω_z = ∂u_v/∂u - ∂u_u/∂v`**

Vorticity `ω_z` is calculated at each grid cell `(i,j)` using central differences:
`ω_z[i][j] = (u_v[i+1][j] - u_v[i-1][j]) / (2·Δu) - (u_u[i][j+1] - u_u[i][j-1]) / (2·Δv)`

### 2.3 Cubed-Sphere Edge Handling

To handle discontinuities across cube face edges, the velocity vector of a neighboring cell on an adjacent face is transformed into the local coordinate system before the finite difference is calculated.

---

## 3. Vertical Pseudo-Velocity (w_c)

### 3.1 Laplacian of Vorticity

The discrete Laplacian of the vorticity field is computed using a 5-point stencil:
`∇²ω_z[i][j] ≈ (ω_z[i+1][j] + ω_z[i-1][j] + ω_z[i][j+1] + ω_z[i][j-1] - 4·ω_z[i][j]) / (Δu²)`

### 3.2 Vertical Velocity Approximation

**Formula:** `w_c[i][j] = -H · ∇²ω_z[i][j]`
**Parameters:** `H = 1000 m` (Characteristic atmospheric height)

---

## 4. Feedback into Buoyancy and Convection

### 4.1 Buoyancy Modification

The standard buoyancy `b` is enhanced by the vertical pseudo-velocity `w_c` to create an effective buoyancy `b_eff`, amplifying updrafts.
`b_eff = b + α_torsion · w_c`

### 4.2 Convection Term Modification

A vortex force term is added to the momentum equation to represent the transport of vorticity.
`du/dt = ... - u · ∇u + ε · (ω × u)`

### 4.3 Temperature Advection Enhancement

A vertical mixing term driven by `w_c` is added to the temperature advection equation.
`∂θ/∂t = -u · ∇θ + β · w_c · (θ_aloft - θ)`

### 4.4 Exact Feedback Coefficients (Locked for v0.3.3)

| Coefficient | Value | Physical Meaning | Source |
|-------------|-------|------------------|--------|
| α_torsion | 0.1 | Buoyancy amplification by w_c | Quick-ref |
| ε (vortex force) | 0.05 | Rotational → linear momentum transfer | Quick-ref |
| β (mixing) | 0.02 | Vertical temperature mixing rate | Quick-ref |

These values are now locked and must not change without a schema version bump.

---

## 5. Integration with Atmospheric Solver (ATMv1)

The torsion module is executed on every atmospheric timestep:
1. Compute horizontal velocity from the main atmospheric solver.
2. Compute the vorticity field `ω_z`.
3. Compute the vertical pseudo-velocity field `w_c`.
4. Feed `w_c` and `ω_z` into the buoyancy and convection terms using the locked coefficients.
5. Solve the atmospheric equations with these modified forcing terms.

---

## 6. Validation and Testing

Validation will be performed using synthetic test cases prior to full integration:
1. **Solid-Body Rotation:** Expect uniform `ω_z` and near-zero `w_c`.
2. **Point Vortex:** Expect a dipole `w_c` pattern and predictable decay.
3. **Shear Layer:** Expect `ω_z` concentrated in the shear layer.

---

## 7. Performance Characteristics

The estimated overhead of the torsion module is **~50 cycles per cell**, representing a **~33-35%** increase in the computational cost of the atmospheric solver. SIMD vectorization and adaptive resolution are planned optimization strategies.

---

## 8. Future Extensions

- **Full 3D Vorticity:** Add `ω_x` and `ω_y` components when a full 3D velocity field becomes available.
- **Vorticity Diffusion:** Add a turbulent eddy viscosity term (`ν_turb · ∇²ω_z`) for dissipation.
- **Coriolis Effect:** Incorporate a β-plane approximation for planetary-scale flows.

---

## 9. Data Structure Specifications

**`TorsionState` Structure:**
```c
typedef struct {
    float** vorticity;          // [grid_size][grid_size]
    float** w_c;                // Vertical pseudo-velocity
    float** laplacian_omega;    // Cached Laplacian
    uint32_t grid_size;
    float du, dv;               // Grid spacing
    float H;                    // Characteristic height (1000 m)
} TorsionState;
```

---

## 10. References

1. **Vorticity Dynamics:** Holton, J.R. "An Introduction to Dynamic Meteorology" (2012), Chapter 5
2. **Cubed-Sphere Grids:** Ronchi et al., "The 'Cubed-Sphere': A new method for the solution of PDEs on the sphere" (1996)
3. **Numerical Weather Prediction:** Durran, D.R. "Numerical Methods for Fluid Dynamics" (2010), Chapter 8
4. **Pseudo-Velocity Approximation:** Verkley & Vosbeek, "Vertical motion from vorticity in quasi-geostrophic flow" (1998)
5. **Kelvin-Helmholtz Instability:** Drazin & Reid, "Hydrodynamic Stability" (2004), Chapter 2

---

## Document Control

- **Approved by:** Technical Lead
- **Review Frequency:** Every major release
- **Next Review:** v0.4.0
