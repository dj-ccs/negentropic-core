# docs/integrators.md — `negentropic-core` v2.2

**Purpose:** precise, implementable reference for structure-preserving integrators (Clebsch / collective Lie–Poisson, RKMK4) and the torsion closure. Includes equations, algorithmic steps, API contracts, test vectors, and implementation notes so engineers can code directly from this document.

---

## 1. High-level goals & invariants

* Preserve physically-important invariants for rotational/vorticity subsystems (Casimirs) and limit energy drift over long integrations.
* Use LoD gating: cheap explicit RK4 at coarse LoD; RKMK4 for SE(3) rotations; Clebsch + symplectic RK for Lie–Poisson vorticity at high LoD.
* Provide deterministic, bounded-latency implementations suitable for WASM and embedded targets with explicit fallbacks when iteration budgets are exceeded.

Key invariants to protect (examples):

* Mass / total moisture (global conserved quantites maintained by solver coupling).
* Casimirs for vorticity bracket (e.g., enstrophy or circulation integrals as defined by discrete bracket).
* Geometric group structure for SE(3) transforms (rotation orthogonality).

---

## 2. Mathematical foundations (compact)

### 2.1 Lie–Poisson form (continuous)

A Lie–Poisson system on (\mathfrak{g}^*) (vorticity-like variable (m)):
[
\dot m = \operatorname{ad}^*_{\delta H/\delta m} m
]
where (H(m)) is Hamiltonian (kinetic energy) and (\operatorname{ad}^*) is coadjoint action. Casimirs (C(m)) satisfy ({C,H}=0).

### 2.2 Clebsch canonization (conceptual)

Find a momentum map (J: T^*Q \to \mathfrak{g}^*) such that (m = J(q,p)). Then integrate canonical Hamiltonian equations on (T^*Q):
[
\dot q = \frac{\partial \mathcal{H}}{\partial p},\quad \dot p = -\frac{\partial \mathcal{H}}{\partial q}
]
with (\mathcal{H}(q,p)=H(J(q,p))). Use a symplectic integrator on ((q,p)) and map back via (J) to obtain the updated Lie–Poisson variable (m). This preserves coadjoint orbits and Casimirs exactly (up to round-off) when using symplectic RK.

### 2.3 RKMK (Runge–Kutta–Munthe–Kaas)

For integrating flows on Lie groups (SE(3) for poses/rotations). RKMK lifts algebra elements (twists) to the group via exponential maps and uses Lie algebra expansions to maintain group structure. RKMK4 is a 4th-order method commonly used for rigid-body attitude.

### 2.4 Discrete torsion closure

Compute discrete curl of the velocity field to produce a torsion (vorticity) vector per cell:
[
\boldsymbol\omega = \nabla \times \mathbf{u} \quad\text{(discrete curl on staggered grid)}
]
Torsion yields physics tendencies (momentum corrections) and cloud-seed enhancement:

* Add momentum tendencies (\Delta \mathbf{u}_{\text{torsion}} \propto \mathbf{curl}(\mathbf{u})) (conservative, small amplitude).
* Increase convective/cloud seed probability (p_{\text{cloud}} \leftarrow p_{\text{cloud}} + \kappa |\omega|).

---

## 3. Algorithms — step-by-step

### 3.1 Clebsch-Collective integrator (runtime pattern)

1. **Identify LP subspace** in cell (discrete vorticity variable `m` and Hamiltonian (H)).
2. **Lift**: compute ((q,p) = \texttt{clebsch_lift}(m)) (closed form or table-driven).
3. **Integrate** canonical pair for dt using a symplectic RK (preferred: 4th-order symplectic PRK or Gauss–Legendre stage 2 implicit); iterate implicit stages with bounded number of Newton iterations `max_iter`. Use "single-Newton" fallback if budget limited.
4. **Project**: (m' = J(q',p')) via `clebsch_project(q',p')`.
5. **Apply conservative correction** if small numerical drift in Casimirs detected (area-weighted scalar correction).
6. **Return** updated `m'`.

Notes:

* If implicit solver fails to converge within `max_iter` or time budget, fall back to an explicit symplectic approximate step (predictor-corrector) and set `NegErrorFlags.INTEGRATOR_FALLBACK`.
* Clebsch lift/projection can be table-accelerated (precompute small matrices for common neighborhood patterns) for speed.

### 3.2 RKMK4 for SE(3) poses

1. Compute Lie algebra element (twist) (\xi) from velocity/angular rates.
2. Evaluate RK stages in algebra using BCH or series expansions as needed.
3. Map to group via exponential map `exp(\xi)`.
4. Compose pose as `g <- g * exp(\xi_dt)`.
5. Enforce orthogonality of rotation (re-orthonormalize if necessary).

### 3.3 Torsion compute & application

* Discrete curl: 3D or 2.5D depending on solver. Use vectorized 5-point or 7-point stencils.
* Compute magnitude and orientation. Write into SAB torsion field for render.
* Compute tendencies: small conservative forcing added to momentum integrator as an extra RHS term.

---

## 4. Data & API contracts (concise)

### 4.1 Integrators API — header (integrators.h)

```c
// integrators.h (contract)
#ifndef NEG_INTEGRATORS_H
#define NEG_INTEGRATORS_H

#include <stdint.h>
#include "state.h"     // SimulationState, GridCell, etc.
#include "workspace.h" // IntegratorWorkspace

typedef enum {
  INTEGRATOR_RK4 = 0,
  INTEGRATOR_SYMPLECTIC_PRK = 1,
  INTEGRATOR_RKMK4 = 2,
  INTEGRATOR_CLEBSCH_COLLECTIVE = 3,
  INTEGRATOR_EXPLICIT_EULER = 4
} integrator_e;

typedef struct {
  double dt;
  int max_iter;         // for implicit solves
  double tol;           // convergence tolerance
  uint32_t flags;       // e.g., preserve_casimirs, allow_approx
} IntegratorConfig;

// Step integrator for a single active cell (or block)
// Returns 0 on success, non-zero error code on fallback/timeout
int integrator_step_cell(GridCell* cell,
                         const IntegratorConfig* cfg,
                         integrator_e method,
                         IntegratorWorkspace* ws);

#endif // NEG_INTEGRATORS_H
```

### 4.2 Torsion API — header (torsion.h)

```c
// torsion.h
#ifndef NEG_TORSION_H
#define NEG_TORSION_H

#include "state.h"

typedef struct {
  float wx, wy, wz;
  float mag;
} neg_torsion_t;

// compute torsion for a rectangular tile of cells.
// Writes torsion into SimulationState.sab_torsion field (if present) and returns 0 on success.
int compute_torsion_tile(SimulationState* S, size_t x0, size_t y0, size_t nx, size_t ny);

// given torsion, compute and apply momentum tendencies to the cell's RHS vector in-place.
void apply_torsion_tendency(GridCell* cell, const neg_torsion_t* t, double dt);

#endif // NEG_TORSION_H
```

### 4.3 Clebsch API — header (clebsch.h)

```c
// clebsch.h
#ifndef NEG_CLEBSCH_H
#define NEG_CLEBSCH_H

#include "state.h"
#include "workspace.h"

// opaque structs for canonical variables or precomputed tables
typedef struct ClebschWorkspace ClebschWorkspace;

// lift LP variable m -> canonical (q,p)
int clebsch_lift(const LPVar* m, ClebschWorkspace* ws, double* q_out, double* p_out);

// project canonical back to LP variable
int clebsch_project(const double* q, const double* p, ClebschWorkspace* ws, LPVar* m_out);

// one symplectic RK step on (q,p), in-place
int clebsch_symplectic_step(double* q, double* p, double dt, const IntegratorConfig* cfg, ClebschWorkspace* ws);

#endif // NEG_CLEBSCH_H
```

---

## 5. Implementation notes, optimizations & fallbacks

### 5.1 Precompute & LUT strategies

* **Clebsch lift** can often be reduced to small linear transforms for local discrete brackets; precompute 8×8 commutator tables for common configurations and store them in read-only data (.rodata) — drastically reduces runtime cost.
* **BCH truncation**: use 2–3 term truncated Baker–Campbell–Hausdorff for small increments in RKMK; fallback to full series only if needed.

### 5.2 Implicit solves

* Use fixed iteration budget (`max_iter`) with simple line-search damping. If not converged, accept single Newton iteration result and flag fallback.
* On WASM, avoid expensive global allocations inside the Newton loop — reuse `Workspace`.

### 5.3 Determinism

* Use integer-based iteration counts and deterministic math ordering in fixed-point mode. Avoid usage of non-deterministic library math (e.g., `pow` with different implementations) — prefer inline, reproducible approximations when fixed-point mode is active.

### 5.4 SIMD & parallelism

* Torsion stencil and RK stage evaluations are data-parallel and should be vectorized (WASM SIMD or CPU intrinsics). Keep per-cell small workspaces to prevent cache thrashing.

---

## 6. Tests & validation (must implement)

* **`integrator_conservation_test.c`**: initialize analytic vorticity field; run N steps with `INTEGRATOR_CLEBSCH_COLLECTIVE` and assert Casimir invariants within tolerance.
* **`integrator_reversibility_test.c`**: forward/backward integration should return to initial state within 1e-10 (FP64 oracle) or within fixed-point tolerance.
* **`integrator_compare_oracle.py`** (CI): run FP64 oracle vs WASM fixed-point for canonical scenario (Loess Plateau 10y), compare hash and energy/Casimir drift; fail if above thresholds.
* **`torsion_unit_test.c`**: analytic velocity -> discrete curl check (L2 error < small epsilon).

---

## 7. File stubs & minimal C code signatures

Below are compact, ready-to-fill C stubs with explanatory comments and clear API signatures. They are intentionally minimal — implementers should expand internals following the comments and the algorithms above.

---

### `src/core/integrators/integrators.h`

```c
// src/core/integrators/integrators.h
#pragma once
#include <stdint.h>
#include "state.h"
#include "workspace.h"

typedef enum {
  INTEGRATOR_RK4 = 0,
  INTEGRATOR_SYMPLECTIC_PRK = 1,
  INTEGRATOR_RKMK4 = 2,
  INTEGRATOR_CLEBSCH_COLLECTIVE = 3,
  INTEGRATOR_EXPLICIT_EULER = 4
} integrator_e;

typedef struct {
  double dt;
  int max_iter;
  double tol;
  uint32_t flags;
} IntegratorConfig;

int integrator_step_cell(GridCell* cell,
                         const IntegratorConfig* cfg,
                         integrator_e method,
                         IntegratorWorkspace* ws);
```

---

### `src/core/integrators/rkmk4.c` (stub)

```c
// src/core/integrators/rkmk4.c
#include "integrators.h"
#include "se3.h"        // SE3 helpers: exp_map, log_map, algebra ops
#include <string.h>

/*
 * RKMK4 for SE(3) twists.
 * - Input: GridCell contains current pose / twist rates
 * - Output: GridCell pose updated in-place
 * NOTE: This implementation must preserve group structure (orthonormal rotation).
 */

static int rkmk4_step_se3(CellPose* pose, const double* twist, double dt, IntegratorWorkspace* ws) {
  // Pseudocode:
  // 1) compute RK stages in Lie algebra using ad-exp series or approximations
  // 2) map to group via exp_map
  // 3) compose pose <- pose * exp_map(stage_increment)
  // 4) re-orthonormalize rotation matrix if necessary
  (void)ws;
  (void)pose;
  (void)twist;
  (void)dt;
  // TODO: implement algebraic stages with BCH truncation for small increments
  return 0;
}

int integrator_step_cell(GridCell* cell,
                         const IntegratorConfig* cfg,
                         integrator_e method,
                         IntegratorWorkspace* ws) {
  if (method != INTEGRATOR_RKMK4) return -1;
  // extract twist/pose from cell
  CellPose* pose = &cell->pose;
  double twist_local[6];
  // fill twist_local from cell rates
  // call step
  return rkmk4_step_se3(pose, twist_local, cfg->dt, ws);
}
```

---

### `src/core/torsion/torsion.c` (stub)

```c
// src/core/torsion/torsion.c
#include "torsion.h"
#include <math.h>
#include <stddef.h>

/*
 * compute_torsion_tile:
 *   Compute discrete curl on tile [x0..x0+nx-1] x [y0..y0+ny-1]
 *   Writes into SimulationState->sab_torsion (if present).
 *
 * Implementation notes:
 *  - Use central differences where available.
 *  - Use safe edge handling (one-sided stencils).
 *  - Keep arithmetic in double for stability, downcast to float for SAB.
 */
int compute_torsion_tile(SimulationState* S, size_t x0, size_t y0, size_t nx, size_t ny) {
  // Read grid spacing
  double dx = S->grid.dx;
  double dy = S->grid.dy;
  for (size_t j = 0; j < ny; ++j) {
    for (size_t i = 0; i < nx; ++i) {
      size_t gx = x0 + i;
      size_t gy = y0 + j;
      GridCell* c = &S->grid.cells[gy * S->grid.nx + gx];
      // fetch neighbor velocities; use central diffs if interior
      double du_dy = 0.0, dv_dx = 0.0;
      // compute components (2.5D case)
      // wx = d/dy(w) - d/dz(v)  (depending on vertical discretization)
      // For 2.5D we compute horizontal curl: omega_z = dv/dx - du/dy
      double dvdx = ( /* neighbor east */ 0.0 );
      double dudy = ( /* neighbor north */ 0.0 );
      double oz = dvdx - dudy;
      neg_torsion_t t;
      t.wx = 0.0f;
      t.wy = 0.0f;
      t.wz = (float)oz;
      t.mag = fabsf(t.wz);
      // write to SAB or to cell->torsion if present
      if (S->sab_torsion) {
        size_t idx = gy * S->grid.nx + gx;
        S->sab_torsion[idx] = t;
      } else {
        c->torsion = t;
      }
    }
  }
  return 0;
}

void apply_torsion_tendency(GridCell* cell, const neg_torsion_t* t, double dt) {
  // Add a small conservative momentum increment proportional to torsion
  // Example: du += alpha * (curl_z) * dt  (scaled by cell area/density)
  const double alpha = 1e-3;
  double factor = alpha * (double)t->mag * dt;
  cell->momentum_u += (float)factor;
  cell->momentum_v += (float)factor;
}
```

---

### `src/core/integrators/clebsch_collective.c` (stub)

```c
// src/core/integrators/clebsch_collective.c
#include "clebsch.h"
#include "integrators.h"
#include <string.h>

/*
 * Minimal clebsch_collective skeleton:
 * - clebsch_lift: map LPVar -> q,p
 * - clebsch_symplectic_step: perform symplectic RK on q,p
 * - clebsch_project: map q,p -> LPVar
 *
 * This file should use precomputed tables when available for speed.
 */

struct ClebschWorkspace {
  // precomputed matrices or LUT handles
  void* lut_handle;
  // temp buffers
  double* tmp_q;
  double* tmp_p;
};

int clebsch_lift(const LPVar* m, ClebschWorkspace* ws, double* q_out, double* p_out) {
  // Example: small linear transform with precomputed matrix M
  // q_out = M_q * m; p_out = M_p * m
  (void)ws;
  (void)m;
  // TODO: implement per-discrete-bracket lift
  return 0;
}

int clebsch_project(const double* q, const double* p, ClebschWorkspace* ws, LPVar* m_out) {
  // inverse transform
  (void)ws;
  (void)q;
  (void)p;
  (void)m_out;
  // TODO: implement
  return 0;
}

int clebsch_symplectic_step(double* q, double* p, double dt, const IntegratorConfig* cfg, ClebschWorkspace* ws) {
  // Use a symplectic PRK or Gauss-Legendre stage solver.
  // Bounded implicit iteration: max_iter = cfg->max_iter
  (void)ws;
  (void)cfg;
  (void)dt;
  // TODO: implement explicit/implicit symplectic RK4
  return 0;
}

/* High-level integrator entry for a cell */
int integrator_step_cell(GridCell* cell, const IntegratorConfig* cfg, integrator_e method, IntegratorWorkspace* ws) {
  if (method != INTEGRATOR_CLEBSCH_COLLECTIVE) return -1;
  LPVar m = cell->lpvar; // copy of LP variable (vorticity, etc)
  ClebschWorkspace cws = {0}; // allocate or use workspace
  double q[/* dimension */ 8];
  double p[/* dimension */ 8];
  clebsch_lift(&m, &cws, q, p);
  clebsch_symplectic_step(q, p, cfg->dt, cfg, &cws);
  clebsch_project(q, p, &cws, &m);
  // write back
  cell->lpvar = m;
  return 0;
}
```

---

## 8. CI expectations and test thresholds

* **Conservation test**: `abs(delta_casimir) < 1e-10` (FP64), `< 1e-6` (fixed-point) after canonical run.
* **Reversibility test**: L2 difference after forward/backwards < 1e-10 (FP64) or fixed-point tolerance.
* **Performance guard**: median ns/cell for Clebsch step on CI runner must be < configured threshold (default 220 ns/cell); exceed → warn and/or run lighter integrator for non-critical LoD.

---

## 9. Implementation priorities (practical)

1. **Torsion kernel** (vectorized) — low risk, big visual & physical payoff. Grok volunteers code.
2. **RKMK4 (SE3)** — medium, reusable for poses and embedded code. Integrate next.
3. **Clebsch lift tables + symplectic RK** — highest impact, moderate risk. Implement LUT-accelerated version first; add implicit solver later.
4. **LoD gating & dynamic escalation** — integrate with scheduler, safety fallbacks.

---

## 10. Next deliverables 

* `docs/integrators_math_appendix.md` — derivations for the discrete Lie–Poisson bracket used here and explicit Clebsch maps for our staggered grid.
* Fully fleshed C implementations of `clebsch_collective.c` and `rkmk4.c` with performance-minded code (SIMD-ready) and small test harnesses.
* Example Python oracle that runs FP64 canonical runs and produces reference hashes.

---
