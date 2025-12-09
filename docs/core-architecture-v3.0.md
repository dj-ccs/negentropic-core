# Negentropic-Core v3.0 — The Genesis Architecture

**Version:** 0.4.0-alpha-genesis
**Date:** 2025-12-09
**Status:** Genesis v3.0 Architectural Pivot

---

## Canonical Eight Principles (v3.0)

The Genesis v3.0 architecture is built on eight foundational principles that override all prior assumptions:

| # | Principle | Description |
|---|-----------|-------------|
| 1 | **Data is the Bottleneck** | Optimize for data throughput, not compute. Memory access patterns dominate performance. |
| 2 | **Decouple Timescales Ruthlessly** | State derivatives must commute across all frequencies. Fast hydrology (hours) and slow regeneration (years) operate independently. |
| 3 | **Sparse is the Default Memory Model** | Grids > 256×256 automatically switch to sparse octree. Target: <300 MB for 100 kha at 1 m resolution. |
| 4 | **Constraints are Energy** | Only smooth, strictly convex, C¹ barrier potentials. No clamps, no if-statements for physical bounds. |
| 5 | **Domain Randomization is Calibration** | Every parameter is a distribution. Ensemble runs validate model robustness. |
| 6 | **Parallel Environments are the Unit of Scale** | Up to 32 workers can run ensemble members concurrently. Scale horizontally. |
| 7 | **Hardware Abstraction from Day One** | Same codebase runs on ESP32-S3, browser WASM, and native. Fixed-point Q16.16 throughout. |
| 8 | **Gradients > Thresholds** | All tipping points become soft score-function gradients. Smooth transitions everywhere. |

---

## Thermodynamic Consistency via Barrier Potentials

### The Problem with Clamps

Traditional numerical solvers use hard clamps to enforce physical constraints:

```c
// BAD: Discontinuous, non-thermodynamic
if (theta < THETA_MIN) theta = THETA_MIN;
```

This creates:
- Discontinuities in the derivative
- Non-physical energy injection/removal
- Numerical instabilities near bounds
- Violations of thermodynamic consistency

### The Genesis Solution: Barrier Potentials

Genesis v3.0 replaces all clamps with smooth, convex barrier potentials:

```c
// GOOD: Smooth, thermodynamically consistent
barrier_gradient = -kappa / (x - x_min + epsilon)^2
dx/dt = physical_derivative + barrier_gradient
```

The barrier potential provides:
- **Smooth energy penalty** as state approaches constraint
- **C¹ continuous derivatives** for stable integration
- **Strict convexity** ensuring unique minima
- **Thermodynamic consistency** through energetic framework

### Implementation

Barrier helpers are defined in `include/barriers.h`:

```c
#define BARRIER_STRENGTH 0x00080000  // 8.0 in Q16.16
#define BARRIER_EPS      0x00000040  // ~0.001

fixed_t fixed_barrier_gradient(fixed_t x, fixed_t x_min);
fixed_t fixed_barrier_gradient_upper(fixed_t x, fixed_t x_max);
fixed_t fixed_barrier_gradient_bounded(fixed_t x, fixed_t x_min, fixed_t x_max);
```

Solvers integrate barrier gradients into the RHS of ODEs:

```c
// In solver derivative computation
float barrier_grad = barrier_gradient_bounded_f(theta, theta_min, theta_max);
d_theta_dt = physical_flux + barrier_grad * damping_factor;
```

---

## Domain Randomization as Calibration

### Principle

Every parameter in the simulation is not a single value but a distribution. This reflects:
- **Measurement uncertainty** in field data
- **Spatial heterogeneity** across landscapes
- **Model structural uncertainty**
- **Epistemic vs aleatoric separation**

### Parameter Schema

Parameters now include statistical moments:

```json
{
  "rainfall_mm": {
    "mean": 450.0,
    "std_dev": 90.0,
    "spatial_corr_length_metres": 5000
  }
}
```

### Gaussian Sampling

Per-cell values are sampled using the 12-sample CLT method (fixed-point compatible):

```c
float sample_gaussian_f(float mean, float std_dev) {
    float sum = 0.0f;
    for (int i = 0; i < 12; i++) {
        sum += uniform_random();  // [0, 1)
    }
    float z = sum - 6.0f;  // Approximate N(0, 1)
    return mean + std_dev * z;
}
```

### CI Validation

The CI oracle runs 100 ensemble members and validates:

```python
# Fail if ensemble std_dev > 4% of mean
if rel_std > 0.04:
    raise SystemExit("FAIL: ensemble exceeds 4% relative std on SOM")
```

---

## Sparse Octree Memory Model

### Automatic Type Selection

Grids automatically select storage type based on size:

```c
typedef enum {
    GRID_UNIFORM = 0,       // Dense array: O(N) memory
    GRID_SPARSE_OCTREE = 1  // Sparse: O(A) where A << N
} GridType;

#define GRID_SPARSE_THRESHOLD (256UL * 256UL)  // 65K cells
```

### Memory Budget

The sparse octree targets <300 MB for 100 kha at 1 m resolution:

- 100 kha = 1,000,000,000 cells
- At 256 bytes/cell dense = 256 GB (infeasible)
- With 0.1% active cells = 256 MB (achievable)

### Current Implementation (Skeleton)

Genesis v3.0 provides the allocation skeleton only:

```c
// Implemented in src/grid/sparse_octree.c
OctreeNode* octree_alloc_root(int nx, int ny, size_t budget_bytes);
int octree_activate_cell(Octree* tree, int i, int j);
int octree_deactivate_cell(Octree* tree, int i, int j);
```

Full traversal and GPU mapping are planned for future sprints.

---

## Parallel Ensemble Execution

### Worker Architecture

The web interface supports up to 32 parallel workers for ensemble calibration:

```typescript
// New message type in core-worker.ts
case 'ENSEMBLE_RUN': {
    const { runs, seed_base, years } = config;
    for (let r = 0; r < runs; r++) {
        const seed = seed_base + r;
        const result = await runSimulationWithSeed(seed, years);
        results.push(result);
    }
    // ...
}
```

### Variance Visualization

The `variance_overlay.ts` module renders per-pixel standard deviation:

```typescript
const overlay = new VarianceOverlay(canvas);
overlay.update(stdDevArray, width, height);
```

Uses CliMA RdBu-11 diverging colormap (blue-white-red) for perceptual uniformity.

---

## API Changes

### New Files

| File | Purpose |
|------|---------|
| `include/barriers.h` | Q16.16 barrier potential helpers |
| `include/grid.h` | Grid abstraction with type selection |
| `src/grid/sparse_octree.h` | Sparse octree data structures |
| `src/grid/sparse_octree.c` | Allocation skeleton |
| `src/grid/grid.c` | Grid lifecycle and access |
| `src/core/parameter_loader.c` | Domain randomization sampling |
| `config/parameters/genesis_crop_params.json` | Example randomized params |
| `web/src/visualization/variance_overlay.ts` | Ensemble variance display |
| `tests/test_barriers.c` | Barrier potential unit tests |
| `tests/test_randomization.c` | CLT sampling unit tests |
| `tests/ci_oracle.py` | Ensemble CI validation |

### Modified Files

| File | Changes |
|------|---------|
| `src/solvers/hydrology_richards_lite.c` | Barrier gradient integration |
| `src/solvers/regeneration_microbial.c` | Barrier gradient integration |
| `web/src/workers/core-worker.ts` | ENSEMBLE_RUN handler |

---

## Backwards Compatibility

### SAB Header

The SharedArrayBuffer header maintains backward-compatible layout. New torsion offsets and octree pointers are optional and ignored by older renderers.

### Solver Interface

Existing solver interfaces are unchanged:

```c
void richards_lite_step(Cell* cells, ...);
void regeneration_cascade_step(Cell* cells, ...);
```

Barrier gradients are integrated internally without changing the API.

### Fixed-Point Arithmetic

All new C code uses Q16.16 fixed-point (no floating-point except Python oracle). This maintains compatibility with embedded targets.

---

## Validation

### Unit Tests

```bash
cd tests
make test_barriers
make test_randomization
./test_barriers
./test_randomization
```

### CI Oracle

```bash
python tests/ci_oracle.py --runs 100 --years 20 --threshold 0.04
```

Expected output:
```
[CI Oracle] OVERALL: PASS
[CI Oracle] Ensemble variance within acceptable bounds
[CI Oracle] Domain randomization calibration validated
```

---

## Roadmap & Next Steps

### Implemented in Genesis v3.0
- [x] Barrier potential framework
- [x] Domain randomization parameter schema
- [x] CLT Gaussian sampling
- [x] Grid type abstraction
- [x] Sparse octree allocation skeleton
- [x] Ensemble worker handler
- [x] Variance overlay visualization
- [x] CI oracle validation

### Future Sprints
- [ ] Spatially correlated sampling (FFT/circulant embedding)
- [ ] Full sparse octree traversal and queries
- [ ] WebGPU compute mapping for sparse grids
- [ ] Replace barrier surrogate with -log LUT if needed
- [ ] Multi-worker parallel ensemble execution

---

## References

### Genesis Principles
1. **Barrier Potentials**: Interior point methods (Boyd & Vandenberghe, 2004)
2. **Domain Randomization**: Tobin et al., "Domain Randomization for Sim2Real" (2017)
3. **Sparse Grids**: Bungartz & Griebel, "Sparse Grids" (2004)

### Existing Documentation
- [Core Architecture v0.1](core-architecture.md) - Original architecture
- [REGv2 Parameters](../config/parameters/REGv2_Microbial.json) - Microbial priming
- [HYD-RLv1 Science](science/microscale_hydrology.md) - Richards-Lite foundation

---

**Document Version:** 3.0
**Last Updated:** 2025-12-09
**Maintainer:** Negentropic Core Contributors
