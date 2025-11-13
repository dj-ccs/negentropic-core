# Negentropic-Core Architecture

**Version:** 0.1.0-alpha
**Status:** Initial port from open-science-dlt
**Last Updated:** 2025-11-14

---

## Overview

**negentropic-core** is the physics and systems engine powering the Negentropic ecosystem. It provides SE(3)-based physical solvers for atmospheric, hydrological, and soil processes, serving both scientific simulation (Python API) and real-time applications (Unity bindings).

This document describes the architectural design, component organization, and implementation status of the core engine.

---

## Design Principles

### 1. **SE(3) Geometric Foundation**
All physical processes operate on SE(3) (Special Euclidean Group in 3D), ensuring:
- Geometric consistency across scales
- Conservation properties through Lie group structure
- Coordinate-free formulations
- Robust numerical integration

### 2. **Modular Physics Solvers**
Physics modules are:
- Independent and composable
- Dynamically loadable
- Coupled through well-defined interfaces
- Testable in isolation

### 3. **Dual-Use Design**
The engine serves:
- **Scientific workflows**: High-accuracy batch simulations with Python API
- **Real-time applications**: Low-latency Unity bindings for Negentropic game
- **Edge computing**: Fixed-point C implementation for ESP32-S3 (separate embedded/ directory)

### 4. **Open Science Provenance**
All code maintains:
- Source attribution (UCF, open-science-dlt)
- Mathematical references
- Validation status
- Integration history

---

## Repository Structure

```
negentropic-core/
├── src/
│   ├── core/                    # SE(3) Lie group mathematics (UCF origin)
│   │   ├── se3_double_scale.py  # Core SE(3) operations & λ optimization
│   │   ├── resonance_aware.py   # Experimental verification cascade
│   │   ├── metrics_service.py   # Main API: compute_regenerative_metrics()
│   │   ├── api_server.py        # Flask REST API wrapper
│   │   ├── client.ts            # TypeScript client
│   │   ├── types.ts             # TypeScript type definitions
│   │   └── tests/               # Unit tests
│   │
│   ├── physics/                 # Physical solvers
│   │   ├── __init__.py          # Module registry
│   │   ├── atmosphere/          # Atmospheric dynamics
│   │   │   └── __init__.py      # AtmosphereSolver
│   │   ├── hydrology/           # Groundwater & surface water
│   │   │   └── __init__.py      # HydrologySolver
│   │   └── soil/                # Soil moisture, nutrients, carbon
│   │       └── __init__.py      # SoilSolver
│   │
│   ├── simulation/              # Orchestration & time integration
│   │   ├── __init__.py
│   │   ├── kernel.py            # SimulationKernel (multi-physics coordinator)
│   │   ├── scheduler.py         # Adaptive time stepping
│   │   └── integrator.py        # SE(3)-aware integrators
│   │
│   ├── io/                      # Data adapters (future)
│   │   └── (NASADEM, HLS, BOM ingestion)
│   │
│   └── utils/                   # Utilities (future)
│       └── (logging, config, math helpers)
│
├── embedded/                    # ESP32-S3 edge computing
│   ├── se3_edge.h               # Fixed-point SE(3) math headers
│   ├── se3_math.c               # Doom-style arithmetic
│   ├── trig_tables.{c,h}        # Trigonometric LUTs
│   ├── t_bsp.{c,h}              # Spatial partitioning
│   └── handoff.c                # Cell handoff protocol
│
├── examples/                    # Demonstration scripts
│   ├── example_usage.py         # SE(3) metrics service examples
│   ├── atmosphere_demo.py       # Atmospheric diffusion demo
│   ├── groundwater_demo.py      # Groundwater flow demo
│   └── soil_moisture_demo.py    # Soil moisture & carbon demo
│
├── tests/                       # C unit tests (embedded)
│   ├── fixed_point_accuracy_test.c
│   └── t_bsp_test.c
│
├── tools/                       # Build utilities
│   ├── generate_trig_lut.py     # Generate trigonometric tables
│   └── verify_trig_lut.py       # Validate LUT accuracy
│
├── docs/
│   └── core-architecture.md     # This file
│
├── config/                      # Simulation configurations (future)
│
├── LICENSE                      # Dual MIT + GPL-3.0
├── README.md
├── pyproject.toml               # Python package config
├── package.json                 # TypeScript/Node.js config
├── tsconfig.json                # TypeScript compiler config
└── requirements.txt             # Python dependencies
```

---

## Core Components

### 1. **SE(3) Lie Group Core** (`src/core/`)

**Origin:** Unified Conscious Evolution Framework (UCF)
**Status:** ✅ Complete and verified
**Mathematical Foundation:** Eckmann & Tlusty (2025), arXiv:2502.14367

#### Purpose
Provides the mathematical substrate for all physical solvers:
- SE(3) pose representation and composition
- Lie algebra operations
- Exponential/logarithmic maps
- λ optimization (regenerative scaling factor)
- Trajectory analysis and return error computation

#### Key Modules
- **`se3_double_scale.py`**: Core SE(3) operations, trajectory composition, λ optimization
- **`resonance_aware.py`**: Experimental resonance detection and multi-level verification
- **`metrics_service.py`**: Public API for computing regenerative metrics from trajectories
- **`api_server.py`**: Flask REST API exposing Python functionality
- **`client.ts`**: TypeScript client for Node.js/Unity integration

#### API Example
```python
from core.metrics_service import compute_regenerative_metrics

trajectory_data = {
    "poses": [
        {"rotation": [0.1, 0, 0], "translation": [0.5, 0, 0]},
        {"rotation": [0, 0.1, 0], "translation": [0, 0.5, 0]}
    ]
}

metrics = compute_regenerative_metrics(trajectory_data)
# Returns: optimal_lambda, return_error_epsilon, verification_score, confidence
```

---

### 2. **Physics Solvers** (`src/physics/`)

**Status:** ⚠️ Stub implementations (v0.1.0-alpha)
**TODO:** Full SE(3)-based physics integration

#### `AtmosphereSolver` (`physics/atmosphere/`)
Atmospheric dynamics on SE(3) manifold:
- **Planned**: Navier-Stokes on Lie groups
- **Planned**: Heat diffusion with geometric consistency
- **Planned**: Moisture transport
- **Planned**: Pressure gradient computation
- **Current**: Stub with basic state management

**State Vector:**
```python
@dataclass
class AtmosphericState:
    temperature: np.ndarray   # (nx, ny, nz) Temperature field (K)
    pressure: np.ndarray      # (nx, ny, nz) Pressure field (Pa)
    wind_velocity: np.ndarray # (nx, ny, nz, 3) Wind vectors (m/s)
    humidity: np.ndarray      # (nx, ny, nz) Relative humidity (0-1)
    timestamp: float          # Unix timestamp
```

#### `HydrologySolver` (`physics/hydrology/`)
Hydrological dynamics on SE(3) manifold:
- **Planned**: Darcy flow on manifold
- **Planned**: Richards equation for vadose zone
- **Planned**: Surface water routing
- **Planned**: Infiltration and percolation
- **Current**: Stub with basic state management

**State Vector:**
```python
@dataclass
class HydrologicalState:
    water_table_depth: np.ndarray   # (nx, ny) Water table depth (m)
    soil_moisture: np.ndarray       # (nx, ny, nz) Volumetric moisture (0-1)
    surface_water: np.ndarray       # (nx, ny) Surface water depth (m)
    groundwater_flow: np.ndarray    # (nx, ny, nz, 3) Flow vectors (m/s)
    timestamp: float                # Unix timestamp
```

#### `SoilSolver` (`physics/soil/`)
Soil dynamics on SE(3) manifold:
- **Planned**: Richards equation for moisture
- **Planned**: Heat equation with phase change
- **Planned**: Nutrient diffusion and uptake
- **Planned**: Carbon decomposition
- **Current**: Stub with basic state management

**State Vector:**
```python
@dataclass
class SoilState:
    moisture: np.ndarray                # (nx, ny, nz) Volumetric moisture
    temperature: np.ndarray             # (nx, ny, nz) Temperature (K)
    nutrients: Dict[str, np.ndarray]    # (nx, ny, nz) per species (kg/m³)
    carbon: np.ndarray                  # (nx, ny, nz) Organic carbon (kg/m³)
    timestamp: float                    # Unix timestamp
```

---

### 3. **Simulation Orchestration** (`src/simulation/`)

**Status:** ⚠️ Stub implementations (v0.1.0-alpha)
**TODO:** SE(3) coupling and advanced time integration

#### `SimulationKernel` (`simulation/kernel.py`)
Multi-physics coordinator:
- Manages multiple physics solvers
- Handles inter-solver coupling (planned)
- Coordinates time stepping
- Manages output and diagnostics

**Usage:**
```python
from simulation import SimulationKernel, SimulationConfig

config = SimulationConfig(
    grid_size=(64, 64, 32),
    dt=60.0,
    max_steps=1000,
    enable_atmosphere=True,
    enable_hydrology=True,
    enable_soil=True
)

kernel = SimulationKernel(config)
kernel.initialize(initial_conditions)
results = kernel.run()
```

#### `Scheduler` (`simulation/scheduler.py`)
Adaptive time stepping:
- Fixed and adaptive time step modes
- Error-based dt adjustment
- CFL condition monitoring (planned)
- Step rejection and retry

#### `SE3Integrator` (`simulation/integrator.py`)
Geometric time integrators:
- **Euler**: Standard forward Euler
- **Lie-Euler**: Exponential map integration
- **RKMK**: Runge-Kutta-Munthe-Kaas
- **Crouch-Grossman**: Intrinsic RK on manifolds

**References:**
- Iserles et al. (2000) "Lie-group methods"
- Crouch & Grossman (1993) "Numerical integration of ODEs on manifolds"
- Munthe-Kaas (1998) "Runge-Kutta methods on Lie groups"

---

### 4. **Embedded Edge Computing** (`embedded/`)

**Platform:** ESP32-S3 (Unexpected Maker ProS3)
**Status:** ✅ Math core complete and verified
**Architecture:** Doom-style fixed-point arithmetic (16.16 format)

#### Purpose
Port of SE(3) λ-estimation to resource-constrained edge devices for:
- Marine AIS trajectory analysis
- Real-time sensor fusion
- Distributed computation

#### Key Components
- `se3_math.c`: Fixed-point arithmetic (mul, div, saturating ops)
- `trig_tables.{c,h}`: 8192-entry sine/cosine LUTs (<1e-4 error)
- `t_bsp.{c,h}`: T-BSP spatial partitioning
- `handoff.c`: Cell-to-cell trajectory handoff

**Accuracy:** <1e-4 vs. floating-point math.h
**Memory:** ~158 KB SRAM, 32 KB LUTs, 50 KB code

---

## Data Flow

### Simulation Workflow

```
Initial Conditions
        ↓
SimulationKernel.initialize()
        ↓
    ┌───────────────────────────────────┐
    │   Time Stepping Loop              │
    │                                   │
    │   Scheduler.compute_next_timestep()
    │           ↓                       │
    │   For each physics solver:        │
    │     SE3Integrator.step()          │
    │       ↓                           │
    │     Solver-specific dynamics      │
    │       (atmosphere/hydrology/soil) │
    │       ↓                           │
    │     Inter-solver coupling (TODO)  │
    │           ↓                       │
    │   Scheduler.advance()             │
    │           ↓                       │
    │   Output at intervals             │
    │           ↓                       │
    └───────────────────────────────────┘
        ↓
Final States + History
```

### SE(3) Metrics Workflow

```
Trajectory Data (Python, TypeScript, or JSON)
        ↓
metrics_service.compute_regenerative_metrics()
        ↓
se3_double_scale.py (SE(3) composition & λ optimization)
        ↓
Optional: resonance_aware.py (verification cascade)
        ↓
RegenerativeMetrics Output
  {
    optimal_lambda,
    return_error_epsilon,
    verification_score,
    confidence,
    metadata
  }
```

---

## Integration Points

### Python API
```python
# SE(3) Metrics
from core.metrics_service import compute_regenerative_metrics

# Physics Solvers
from physics import AtmosphereSolver, HydrologySolver, SoilSolver

# Simulation
from simulation import SimulationKernel, SimulationConfig
```

### TypeScript/Node.js
```typescript
import { scienceClient } from './core/client';

const metrics = await scienceClient.computeMetrics(trajectoryData);
```

### REST API
```bash
curl -X POST http://localhost:5000/api/v1/science/metrics \
  -H "Content-Type: application/json" \
  -d '{"trajectory_data": {...}}'
```

### Unity (Planned)
- C# bindings to Python via REST API or native DLL
- Real-time state queries
- Asynchronous simulation updates

---

## Implementation Status

### ✅ Complete
- SE(3) core mathematics (from UCF)
- Flask REST API wrapper
- TypeScript client
- Project structure and configuration
- Dual MIT + GPL licensing
- Embedded fixed-point math core
- Example demonstrations

### ⚠️ Stub / In Progress
- Physics solvers (atmosphere, hydrology, soil)
- Simulation kernel coupling
- SE(3) time integrators (Lie-Euler, RKMK, CG)
- Adaptive time stepping

### 📋 TODO
- Full SE(3)-based physics implementations
- Data I/O adapters (NASADEM, HLS, BOM)
- Unity C# bindings
- Comprehensive test suite
- Performance profiling
- Documentation and tutorials

---

## Dependencies

### Python
- `numpy>=1.24.0` - Numerical arrays
- `scipy>=1.10.0` - Optimization and scientific computing
- `flask>=3.0.0` - REST API server
- `flask-cors>=4.0.0` - CORS support
- `pydantic>=2.0.0` - Data validation
- `pyyaml>=6.0.0` - Configuration files
- `pytest>=7.4.0` - Testing (dev)

### TypeScript/Node.js
- `axios` - HTTP client
- `typescript>=5.3.0` - TypeScript compiler (dev)

### Embedded (C)
- No external dependencies (standalone)
- ESP-IDF for ESP32-S3 deployment

---

## Provenance

### Mathematical Foundation
- **Source:** Unified Conscious Evolution Framework (UCF)
- **Repository:** https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework
- **Reference:** Eckmann & Tlusty (2025), arXiv:2502.14367
- **Integration Date:** 2025-11-11

### Codebase Origin
- **Ported From:** open-science-dlt
- **Repository:** https://github.com/dj-ccs/open-science-dlt
- **Migration Date:** 2025-11-14
- **Purpose:** Separate simulation engine from DLT platform

---

## Testing

### Unit Tests (Python)
```bash
cd src/core/tests
pytest test_metrics_service.py -v
```

### Embedded Tests (C)
```bash
cd tests
make test
```

### Examples
```bash
# SE(3) metrics
python examples/example_usage.py

# Physics demos
python examples/atmosphere_demo.py
python examples/groundwater_demo.py
python examples/soil_moisture_demo.py
```

---

## Future Work

### Phase 1: Core Physics (Q1 2026)
- Implement full SE(3)-based Navier-Stokes (atmosphere)
- Implement Darcy flow on manifold (hydrology)
- Implement Richards equation (soil moisture)
- Couple solvers via SE(3) transformations

### Phase 2: Validation (Q2 2026)
- Benchmark against standard solvers (WRF, MODFLOW, HYDRUS)
- Field trial validation (agricultural applications)
- Performance profiling and optimization

### Phase 3: Integration (Q3 2026)
- Unity C# bindings
- Data I/O adapters (NASA, BOM, USGS)
- Real-time visualization
- Edge deployment (ESP32-S3 λ-estimation)

### Phase 4: Ecosystem (Q4 2026)
- Negentropic game integration
- Multi-scale coupling (global → local)
- Machine learning integration (surrogate models)
- Open science platform publishing

---

## References

1. **Eckmann & Tlusty (2025)** - SE(3) Double-and-Scale Regenerative Return, arXiv:2502.14367
2. **Iserles et al. (2000)** - Lie-group methods, Acta Numerica
3. **Crouch & Grossman (1993)** - Numerical integration of ODEs on manifolds
4. **Munthe-Kaas (1998)** - Runge-Kutta methods on Lie groups
5. **UCF Repository** - https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework
6. **Open Science DLT** - https://github.com/dj-ccs/open-science-dlt
7. **Doom Source Code** - https://github.com/id-Software/DOOM (fixed-point math reference)

---

## License

Dual-licensed under MIT OR GPL-3.0-or-later. See `LICENSE` file for details.

---

**Document Version:** 1.0
**Last Updated:** 2025-11-14
**Maintainer:** Negentropic Core Contributors
