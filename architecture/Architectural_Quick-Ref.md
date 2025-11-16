# negentropic-core v0.3.3 Architectural Quick Reference

**Version:** v0.3.3
**Last Updated:** 2025-11-16
**Status:** Production Ready

---

## Purpose

This document serves as the master index for the negentropic-core architecture documentation. It provides direct links to the nine interconnected technical specifications that define the exact, unambiguous implementation details for the development team.

Each specification document provides production-ready implementation details including:
- Exact algorithms and mathematical formulas
- Data structures with byte-level layouts
- API specifications and function signatures
- Performance targets and validation criteria
- Reference implementations and test cases

---

## Document Organization

The architecture documentation is organized into nine core specifications:

1. **Deterministic Numerics** - Computational reproducibility foundations
2. **Memory Architecture** - Zero-copy data pipeline and SAB layout
3. **Core Physics** - HYD-RLv1, REGv1/v2, SE(3) mathematics
4. **Torsion Tensor** - 2.5D vorticity simulation module
5. **Atmospheric & Cloud** - Dynamic cloud system and biotic pump
6. **LoD & Cascading** - Multi-scale simulation engine
7. **Event Logging** - Hash-chained audit trail standard
8. **CI/CD Validation** - Oracle-based testing protocol
9. **Platform Interop** - Unity and Web integration

---

## Master Table of Contents

### 1. **[Deterministic Numerics Standard](./Deterministic_Numerics_Standard_v0.3.3.md)**

**Purpose:** To define the non-negotiable rules for ensuring perfect, cross-platform reproducibility.

**Key Contents:**
- **Fixed-Point Arithmetic:** Q16.16 format specification, overflow handling, error bounds
- **Rounding Modes:** IEEE 754 round-to-nearest enforcement
- **Transcendental Functions:**
  - Trigonometric LUTs (8192 entries, <1×10⁻⁴ error)
  - Exponential/logarithm approximations (256 entries)
  - Van Genuchten hydraulic LUTs (256 entries, ~13× speedup)
- **NegRNG Specification:** xorshift64* PRNG, uniform/Gaussian distributions
- **Adaptive LUT Refinement:** Runtime error tracking and bisection refinement
- **Validation:** 39/39 unit tests passing, cross-platform hash validation

**Status:** ✅ Production Ready

---

### 2. **[Memory Architecture and SAB Layout](./Memory_Architecture_and_SAB_Layout_v0.3.3.md)**

**Purpose:** To provide the concrete memory schema for the zero-copy data pipeline.

**Key Contents:**
- **Single Contiguous Memory Block:** Header + SE(3) poses + scalar fields
- **SharedArrayBuffer Header:** 128-byte cache-aligned structure
  - Control signals (atomic int32)
  - Simulation metadata (timestamp, grid dimensions)
  - Field offsets (theta, SOM, vegetation, etc.)
  - Double-buffering flags
- **SE(3) Pose Structure:** 64-byte aligned, quaternion normalization protocol
- **Synchronization Protocol:** Atomics API for 3-thread architecture
  - Main Thread: CesiumJS + UI
  - Core Worker: Physics (10 Hz)
  - Render Worker: deck.gl (60 FPS)
- **Alignment & Endianness:** 16-byte SIMD alignment, little-endian mandate
- **Performance:** <0.3 ms SAB write, <0.5 µs atomic notify

**Status:** ✅ Production Ready

---

### 3. **[Core Physics Specification](./Core_Physics_Specification_v0.3.3.md)**

**Purpose:** To detail the mathematical implementation of the core physics solvers.

**Key Contents:**
- **HYD-RLv1 (Richards-Lite Hydrology):**
  - Unified Richards equation (Hortonian + Dunne runoff)
  - Fill-and-spill microtopography: `C(ζ) = 1/(1 + exp[-a_c(ζ - ζ_c)])`
  - Intervention multipliers: gravel mulch (6×), swales (50-70% reduction)
  - Van Genuchten parameterization with 256-entry LUTs
  - D8/D∞ flow routing algorithms
  - Performance: <20 ns/cell (with LUTs)

- **REGv1 (Regeneration Cascade):**
  - Vegetation: `dV/dt = r_V·V·(1 - V/K_V) + λ1·max(θ - θ*, 0) + λ2·max(SOM - SOM*, 0)`
  - SOM: `dSOM/dt = a1·V - a2·SOM`
  - Hydrological feedback: porosity (+5mm/1% SOM), conductivity (1.15^dSOM)
  - Validation: 36/38 tests (94.7%), Loess Plateau 1995-2010 calibration

- **REGv2 (Microbial Priming):**
  - F:B ratio priming (8-entry LUT, 1.0× to 10× SOM production)
  - Aggregation-conductivity coupling
  - Condensation flux (fog/dew, 0.1-5 mm/d with rock mulch bonus)
  - Hydraulic lift (0.1-1.3 mm/night, nocturnal gate)
  - Validation: 44/49 tests (90%)

- **SE(3) Tile-Local Frames:**
  - Cubed-sphere coordinate system (6 faces, avoids polar singularities)
  - Quaternion transform functions (ECEF ↔ local)
  - 16-byte aligned, <20 ns/transform

**Status:** ✅ Production Ready

---

### 4. **[Torsion Tensor Module](./Torsion_Tensor_Module_v0.3.3.md)**

**Purpose:** To specify the complete physics and implementation of the "2.5D" vorticity simulation.

**Key Contents:**
- **Discrete Curl Operator:** Cubed-sphere finite differences
  - `ω_z = ∂u_v/∂u - ∂u_u/∂v`
  - Edge transformation for velocity continuity across cube faces

- **Vertical Pseudo-Velocity:**
  - `w_c = -H · ∇²ω_z` (H = 1000 m characteristic height)
  - 5-point stencil Laplacian

- **Feedback Mechanisms:**
  - Buoyancy: `b_eff = b + α_torsion·w_c` (α = 0.1)
  - Vortex force: `du/dt += ε·(ω × u)` (ε = 0.05)
  - Temperature mixing: `∂θ/∂t += β·w_c·(θ_aloft - θ)` (β = 0.02)

- **Performance:** ~50 cycles/cell (~35% overhead)
- **Validation:** Synthetic test cases (solid-body rotation, point vortex, shear layer)

**Status:** 🚧 Planned (awaiting ATMv1 completion)

---

### 5. **[Atmospheric and Cloud System](./Atmospheric_and_Cloud_System_v0.3.3.md)**

**Purpose:** To define the visual and physical coupling of the dynamic cloud layer.

**Key Contents:**
- **Cloud Particle Advection (Lagrangian):**
  - Position: `dx/dt = u + w_turb`, `dz/dt = w_c + w_buoyancy`
  - Moisture: `dm/dt = γ_cond·max(q - q_sat, 0) - γ_evap·m - γ_precip·m²`
  - RK4 integration for smooth trajectories

- **Noise Domain Warping:**
  - Multi-octave Perlin noise (4 octaves, domain warping)
  - Turbulent wind field for realistic cloud motion

- **deck.gl Custom Layer:**
  - Vertex shader: Billboard particles (camera-facing)
  - Fragment shader: Radial falloff, moisture-based color, age fade
  - Performance target: 60 FPS @ 100k particles

- **Atmospheric Coupling:**
  - Condensation: Atmosphere → Clouds (humidity threshold)
  - Evaporation: Clouds → Atmosphere (5% per second)
  - Precipitation: Clouds → Surface (quadratic autoconversion)

- **Biotic Pump (Optional):**
  - Vegetation-enhanced condensation: `γ_cond_eff = γ_cond·(1 + 2.0·V)`
  - Forest continuity threshold (V > 0.6, area > 100 km²)

**Status:** 🚧 Planned (awaiting ATMv1 + deck.gl integration)

---

### 6. **[LoD and Cascading Simulation System](./LoD_and_Cascading_Simulation_System_v0.3.3.md)**

**Purpose:** To specify the complete mechanics of the multi-scale simulation engine.

**Key Contents:**
- **Temporal Cascading:**
  - HYD-RLv1: 1 hour timestep, every frame (10 Hz)
  - REGv1/v2: ~5.3 days, every 128 HYD steps (~99.2% call reduction)
  - ATMv1: 10 seconds, every frame
  - Accumulation buffers for slow-scale averaging

- **Spatial Level-of-Detail (Quad-Tree):**
  - Level 0: 16×16 (coarse, 100 km spacing)
  - Level 1: 32×32 (50 km)
  - Level 2: 64×64 (25 km)
  - Level 3: 128×128 (fine, 12.5 km)

- **Camera-Driven Refinement:**
  - Distance threshold: <50 km (refine), >75 km (coarsen)
  - Feature importance: `I = |∇θ| + |∇V| + |∇SOM| + α·runoff`
  - Hysteresis state machine (prevents flickering)

- **Conservative Flux Transfer:**
  - Upscaling: Average intensive (θ), sum extensive (runoff)
  - Downscaling: Bilinear interpolation (smooth fields), conservative distribution (runoff)
  - Nonlinearity handling: Average K(θ), not K(avg θ)

- **Smooth Transitions:** 30-frame blend (3 seconds at 10 Hz)
- **Performance:** ~10× cell reduction with 3-level LoD, <5% overhead

**Status:** ✅ Temporal Cascade, 🚧 Spatial LoD (planned)

---

### 7. **[User Events and Hash-Chained Log Standard](./User_Events_and_Hash_Chained_Log_Standard_v0.3.3.md)**

**Purpose:** To define the standard for auditable, replayable user interactions.

**Key Contents:**
- **Event Schema (JSON):**
  - Base fields: event_id, timestamp_us, event_type, session_id, user_id
  - Cryptographic chain: prev_hash, hash (SHA-256)
  - Versioned payload (schema_version: 1)

- **Event Types:**
  - session_start, session_end
  - place_intervention, remove_intervention, change_parameter
  - camera_move, simulation_step, checkpoint

- **Canonical Serialization:**
  - Alphabetical field order (enforced)
  - Compact JSON (no whitespace)
  - Float precision: 6 decimals

- **SHA-256 Hash Chain:**
  - Genesis event: prev_hash = "0000...0000"
  - Each event hashes previous event
  - Tamper detection: Any modification invalidates chain

- **Timestamp Resolution:** Microseconds (±1 µs), monotonic guarantee
- **Storage:** NDJSON (newline-delimited JSON), optional LZ4 compression
- **Replay Protocol:** Deterministic reconstruction from event log + seed
- **Performance:** <56 µs per event (<1% of 10 Hz frame budget)

**Status:** ✅ Production Ready

---

### 8. **[CI Oracle and Validation Protocol](./CI_Oracle_and_Validation_Protocol_v0.3.3.md)**

**Purpose:** To provide the exact parameters for the automated continuous integration and validation pipeline.

**Key Contents:**
- **Canonical Scenario: "Loess Plateau 10-Year"**
  - Region: Yan'an Prefecture, China (36.5°N, 109.5°E)
  - Time: 1995-2005 (3,653 days)
  - Grid: 100×100 cells, 100 m spacing
  - Elevation: 800-1,200 m (synthetic DEM)

- **Initial Conditions (t = 0):**
  - Soil moisture: θ = 0.12 (degraded, 4-layer profile)
  - SOM: 0.8% (heavily degraded)
  - Vegetation: 15% cover (sparse grassland)
  - Hydraulic params: Van Genuchten (Li et al. 2003)

- **Climate Forcing (1995-2005):**
  - Precipitation: 450 mm/yr (exponential daily distribution)
  - Temperature: 10°C mean, ±15°C seasonal amplitude
  - PET: ~800 mm/yr (Hargreaves equation)
  - Format: CSV (daily resolution, 3,653 rows)

- **Intervention Schedule:**
  - Baseline: No interventions (control)
  - Restoration: Gravel mulch (1996), swales (1997), check dams (1998), terracing (2000), trees (2002)
  - Expected outcomes: +50-60% vegetation, +1.2-1.7% SOM, -50% runoff

- **Oracle Implementation:**
  - Language: Python 3.10+, NumPy float64
  - Purpose: Ground truth for C implementation
  - Validation: Hash comparison (daily checkpoints), max error <1×10⁻⁴

- **CI/CD Pipeline (GitHub Actions):**
  - Pre-flight smoke test
  - Run Python oracle (10 years)
  - Build C core, run simulation
  - Compare state hashes (all must match)
  - Performance benchmark (<60 s for 10 years)

**Status:** ✅ Production Ready

---

### 9. **[Unity and Web Interop Specification](./Unity_and_Web_Interop_Specification_v0.3.3.md)**

**Purpose:** To ensure the physics engine remains consistent between the Unity game and the web-based Planetary Control Panel.

**Key Contents:**
- **Dual-Platform Architecture:**
  - Unity: C# P/Invoke → native DLL (Windows/macOS/Linux)
  - Web: TypeScript → WASM (Emscripten)
  - Determinism guarantee: Same seed → identical state (both platforms)

- **Shared State Serialization:**
  - Binary format: 64-byte header + scalar fields + SE(3) poses + event log
  - Endianness: Little-endian (all platforms)
  - JSON format: Human-readable (debugging, diffs)

- **Unity C# Integration:**
  - P/Invoke API: `neg_create`, `neg_destroy`, `neg_step`, `neg_get_field_float32`
  - C# wrapper: Managed `NegenotropicCore` class
  - DLL deployment: `Assets/Plugins/x86_64/` (platform-specific)
  - Performance: ~10% overhead (2.6 ms per frame)

- **Web WASM Integration:**
  - Emscripten build: `-O3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1`
  - TypeScript wrapper: `NegenotropicCore` class (cwrap/ccall)
  - Web Worker: Background physics (10 Hz), zero-copy SAB
  - Performance: 1.5× slower than native (SIMD optimizations available)

- **Seed and Event Log Sharing:**
  - Unity → Web: Export JSON (seed + config)
  - Web → Unity: Import event log (NDJSON)
  - Cross-platform validation: Hash checkpoints (must match)

- **Testing:**
  - Unit tests: NUnit (Unity), Jest (Web)
  - Integration: GitHub Actions (Windows/macOS/Linux)
  - Cross-validation: Python script (compare Unity vs. Web hashes)

**Status:** ✅ Web, 🚧 Unity (P/Invoke planned)

---

## Quick Navigation by Topic

### Physics & Mathematics
- [Core Physics Specification](./Core_Physics_Specification_v0.3.3.md) - HYD, REG, SE(3)
- [Deterministic Numerics Standard](./Deterministic_Numerics_Standard_v0.3.3.md) - Fixed-point, LUTs, RNG
- [Torsion Tensor Module](./Torsion_Tensor_Module_v0.3.3.md) - Vorticity, vertical velocity
- [Atmospheric and Cloud System](./Atmospheric_and_Cloud_System_v0.3.3.md) - Clouds, biotic pump

### System Architecture
- [Memory Architecture and SAB Layout](./Memory_Architecture_and_SAB_Layout_v0.3.3.md) - Zero-copy pipeline
- [LoD and Cascading Simulation System](./LoD_and_Cascading_Simulation_System_v0.3.3.md) - Multi-scale engine

### Validation & Testing
- [CI Oracle and Validation Protocol](./CI_Oracle_and_Validation_Protocol_v0.3.3.md) - Automated testing
- [User Events and Hash-Chained Log Standard](./User_Events_and_Hash_Chained_Log_Standard_v0.3.3.md) - Audit trail

### Platform Integration
- [Unity and Web Interop Specification](./Unity_and_Web_Interop_Specification_v0.3.3.md) - Cross-platform

---

## Implementation Status Summary

| Component | Status | Tests Passing | Performance |
|-----------|--------|---------------|-------------|
| HYD-RLv1 | ✅ Production | TBD | <20 ns/cell ✅ |
| REGv1 | ✅ Production | 36/38 (94.7%) | 20-30 ns/cell ✅ |
| REGv2 | ✅ Production | 44/49 (90%) | 40-48 cycles/cell ✅ |
| Fixed-Point Math | ✅ Production | 39/39 (100%) | 2× speedup ✅ |
| SE(3) Transforms | ✅ Production | 15/15 (100%) | 15-18 ns/op ✅ |
| Event Logging | ✅ Production | 100% | <56 µs/event ✅ |
| Oracle Validation | ✅ Production | 100% | <60 s (10 yr) ✅ |
| Web WASM | ✅ Production | 100% | 1.5× native ✅ |
| Memory/SAB | ✅ Production | 100% | <0.3 ms write ✅ |
| Temporal Cascade | ✅ Production | 100% | 99.2% reduction ✅ |
| ATMv1 | 🚧 Planned | N/A | <10 ns/cell (target) |
| Torsion Tensor | 🚧 Planned | N/A | ~50 cycles/cell (est) |
| Cloud Particles | 🚧 Planned | N/A | 60 FPS @ 100k (target) |
| Spatial LoD | 🚧 Planned | N/A | 10× reduction (est) |
| Unity P/Invoke | 🚧 Planned | N/A | 10% overhead (est) |

---

## Key Performance Metrics

**Target Hardware:** x86-64, 16 GB RAM, SSD storage

| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| Simulation Speed | 60 sim-years/hr | >60 sim-years/hr | ✅ |
| Memory Usage | <500 MB | <200 MB | ✅ |
| Frame Rate (Web) | 60 FPS | 60 FPS | ✅ |
| Physics Frequency | 10 Hz | 10 Hz | ✅ |
| Event Log Overhead | <1% | <1% | ✅ |
| WASM Load Time | <2 s | 1.2 s | ✅ |
| Cross-Platform Error | <1×10⁻⁴ | <1×10⁻⁴ | ✅ |

---

## External References

### Scientific Foundation
- **Hydrology:** Weill et al. (2009), Frei et al. (2010), Li et al. (2003)
- **Regeneration:** Cao et al. (2011), Johnson-Su compost (multiple sources)
- **Biotic Pump:** Makarieva & Gorshkov (2007)

### Technical Standards
- **IEEE 754:** Floating-point arithmetic (2019)
- **SHA-256:** FIPS PUB 180-4 (2015)
- **JSON:** RFC 8259, NDJSON specification
- **WebAssembly:** W3C specification (2023)

### Software Engineering
- **Emscripten:** Zakai (2011)
- **Unity Burst:** Unity Technologies (2023)
- **Verification & Validation:** Oberkampf & Roy (2010)

---

## Document Maintenance

**Version Control:**
- All architecture documents are version-controlled in Git
- Changes require PR approval from Technical Lead
- Breaking changes increment schema_version in affected documents

**Review Schedule:**
- Architecture review: Every release (alpha, beta, production)
- Performance validation: Every sprint (2 weeks)
- Cross-platform testing: Every commit (CI/CD)

**Next Review:** v0.4.0-alpha (Q1 2025)

---

## Contact & Support

**Technical Lead:** [Contact via project repository]

**Documentation Issues:** [GitHub Issues](https://github.com/dj-ccs/negentropic-core/issues)

**Discussion:** [GitHub Discussions](https://github.com/dj-ccs/negentropic-core/discussions)

---

**Last Updated:** 2025-11-16 | **Document Version:** 1.0 | **Schema Version:** v0.3.3
