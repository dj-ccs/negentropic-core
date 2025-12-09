# Changelog

All notable changes to negentropic-core will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.0-alpha-genesis] - 2025-12-09

### Genesis v3.0 Architectural Pivot

This release implements the Genesis v3.0 architectural pivot, elevating negentropic-core from a fast deterministic simulator to the world's first thermodynamically consistent, uncertainty-aware, planet-scale ecological engine.

### Canonical Eight Principles

1. **Data is the Bottleneck** - Optimize for data throughput, not compute
2. **Decouple Timescales Ruthlessly** - State derivatives commute across frequencies
3. **Sparse is the Default Memory Model** - Auto-switch to octree for large grids
4. **Constraints are Energy** - Smooth barrier potentials replace all clamps
5. **Domain Randomization is Calibration** - Every parameter is a distribution
6. **Parallel Environments are the Unit of Scale** - Up to 32 ensemble workers
7. **Hardware Abstraction from Day One** - Q16.16 fixed-point throughout
8. **Gradients > Thresholds** - Soft score-function gradients for tipping points

### Added

#### [PHYS-v3.1] Unbreakable Solver Sprint

- **Barrier Potentials** (`include/barriers.h`)
  - Q16.16 fixed-point barrier helper functions
  - `fixed_barrier_gradient()` for lower bounds
  - `fixed_barrier_gradient_upper()` for upper bounds
  - `fixed_barrier_gradient_bounded()` for combined constraints
  - Thermodynamically consistent constraint enforcement

- **Domain Randomization** (`src/core/parameter_loader.c`)
  - CLT-based Gaussian sampling (12-sample method, fixed-point compatible)
  - `sample_gaussian_f()` and `sample_gaussian_fixed()` functions
  - RNG with deterministic seeding for reproducibility
  - Per-cell parameter sampling for ensemble runs

- **Parameter Schema** (`config/parameters/genesis_crop_params.json`)
  - Extended JSON schema with `mean`, `std_dev`, `spatial_corr_length_metres`
  - Example parameters for hydrology, vegetation, soil, microbial, atmospheric

#### [GEO-v3.1] Infinite Earth Sprint

- **Sparse Memory Model** (`include/grid.h`, `src/grid/`)
  - `GridType` enum: `GRID_UNIFORM` and `GRID_SPARSE_OCTREE`
  - Auto-switch to sparse octree for grids > 256×256
  - Memory budget targeting <300 MB for 100 kha at 1 m resolution
  - Allocation skeleton (traversal planned for future sprint)

- **Parallel Ensemble Mode** (`web/src/workers/core-worker.ts`)
  - `ENSEMBLE_RUN` message type for ensemble calibration
  - Progress updates via `ENSEMBLE_PROGRESS`
  - Statistics computation with 4% threshold validation
  - Support for up to 32 parallel workers

- **Variance Overlay** (`web/src/visualization/variance_overlay.ts`)
  - Per-pixel standard deviation visualization
  - CliMA RdBu-11 compatible diverging colormap
  - `VarianceOverlay` class with legend support
  - `computeEnsembleStdDev()` helper function

#### Tests

- `tests/test_barriers.c` - Barrier potential unit tests
- `tests/test_randomization.c` - CLT sampling unit tests
- `tests/ci_oracle.py` - Ensemble validation CI script (100 runs, 4% threshold)

#### Documentation

- `docs/core-architecture-v3.0.md` - Genesis v3.0 architecture specification
- Eight principles table at document top
- Sections for barrier potentials, domain randomization, sparse octree, ensemble execution

### Changed

- **Hydrology Solver** (`src/solvers/hydrology_richards_lite.c`)
  - Integrated barrier gradients into vertical implicit solver RHS
  - Added barrier contribution to horizontal explicit pass
  - Added barrier gradient to evaporation computation
  - Safety backstops retained for extreme numerical conditions

- **Microbial Solver** (`src/solvers/regeneration_microbial.c`)
  - Added barrier gradient helpers
  - `regv2_soft_positive()` for C¹ continuous positive part function
  - Swale storage update with barrier contribution
  - Version bumped to 0.4.0-alpha-genesis

- **README.md**
  - Added v3.0_Genesis architecture badge

- **CMakeLists.txt**
  - Version bumped to 0.4.0-alpha-genesis

- **package.json** (root and web)
  - Version bumped to 0.4.0-alpha-genesis

### Technical Notes

- All C code remains C99 + Q16.16 fixed-point (no floating-point except Python oracle)
- All new functions are pure and side-effect free
- Sparse octree provides allocation skeleton only (no traversal per constraint)
- Spatial correlation in parameters is parsed but not yet implemented (reserved)

---

## [0.3.0-alpha] - 2025-11-19

### GEO-v1 Production Sprint Complete

- Impact Map + Cesium Primitives shipping
- Architectural pivot ORACLE-005 → ORACLE-008 complete
- 60 FPS locked visualization

### Added

- `web/` directory with Cesium-based GEO-v1 interface
- SharedArrayBuffer zero-copy pipeline
- CliMA RdBu-11 impact colormap
- Click-to-fly navigation

---

## [0.2.0-alpha] - 2025-11-14

### REGv2 Sprint Complete

- Microbial priming & condenser landscapes
- Johnson-Su explosive recovery validated (50× SOM in 2 years)
- 44/49 tests passing (90%)

### Added

- `regeneration_microbial.c` - REGv2 solver
- `REGv2_Microbial.json` - Parameter set with 11 citations
- 8-entry F:B lookup table with hard anchors

---

## [0.1.0-alpha] - 2025-11-14

### Initial Release

- SE(3) Lie group mathematics from UCF
- HYD-RLv1 Richards-Lite hydrology solver
- REGv1 Regeneration cascade solver
- Fixed-point Q16.16 arithmetic for ESP32-S3
- van Genuchten LUTs (~13× speedup)

### Added

- Core physics solvers
- Embedded fixed-point implementation
- Python API server
- TypeScript client
- Example demonstrations
