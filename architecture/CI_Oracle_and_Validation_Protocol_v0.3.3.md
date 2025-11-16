# CI Oracle and Validation Protocol v0.3.3

**Version:** v0.3.3
**Status:** ✅ Production Ready
**Last Updated:** 2025-11-17

## Purpose

This document provides the exact parameters for the automated continuous integration and validation pipeline. It defines the precise domain bounds, initial conditions (soil profiles, SOM fractions), climate forcing time-series, and intervention schedule for the "Loess Plateau 10-Year" canonical scenario used for CI/CD validation.

---

## 0. Current Status (from Architecture Quick Reference)

| Item | Status | Notes |
|------|--------|-------|
| Python FP64 Oracle | ✅ Production | NumPy float64 ground truth |
| Canonical Scenario | ✅ Production | Loess Plateau 1995–2005, 100×100 @ 100 m |
| Daily Hash Checkpoints | ✅ Production | All 3,653 days must match |
| Max Error Tolerance | ✅ Production | < 1×10⁻⁴ relative error |
| Performance Target | ✅ Production | < 60 s for 10 years on CI runner |

---

## 1. Overview

### 1.1 Validation Strategy

The validation framework operates on three levels:

1. **Unit Tests:** Verify component-level correctness (solvers, data structures).
2. **Integration Tests:** Verify multi-component interactions.
3. **Oracle Validation:** Compare the full production C simulation against a high-precision Python FP64 reference implementation (the "Oracle").

This three-tier approach ensures both micro-level algorithmic correctness and macro-level system behavior.

### 1.2 CI/CD Pipeline

A GitHub Actions workflow automatically triggers on every push and pull request. The build fails if any of the following occur:
- Unit test failure
- Hash mismatch between the C and Python runs
- Precision degradation beyond the allowed tolerance
- Performance regressions

**Pipeline Stages:**
```
1. Build → 2. Unit Tests → 3. Run Python Oracle → 4. Run C Implementation → 5. Compare Results → 6. Performance Check
```

### 1.3 Design Philosophy

The validation system is designed to be:
- **Deterministic:** Same inputs always produce identical outputs (bit-for-bit).
- **Comprehensive:** Tests cover all physics modules and their interactions.
- **Fast:** Entire validation suite runs in < 3 minutes on standard CI hardware.
- **Auditable:** All test data and results are versioned and archived.

---

## 2. Canonical Scenario: "Loess Plateau 10-Year"

### 2.1 Scenario Parameters

- **Region:** Yan'an Prefecture, Shaanxi Province, China (36.5°N, 109.5°E)
- **Time Period:** 1995-01-01 to 2005-01-01 (3,653 days)
- **Grid:** 100×100 cells at 100m spacing (1 km × 1 km domain)
- **DEM:** Synthetic, deterministically generated DEM for reproducibility
- **Simulation Frequency:** 10 Hz (0.1 s real-time = 1 hour sim-time)
- **Total Timesteps:** 876,720 (10 years × 365.25 days × 24 hours × 10 Hz)

### 2.2 DEM Generation

The DEM is generated using a deterministic Perlin noise function seeded with `0x4C4F455353` ("LOESS" in hex).

**Canonical DEM Generator:**
```python
import numpy as np

def generate_loess_plateau_dem(nx=100, ny=100, seed=0x4C4F455353):
    """Generate synthetic Loess Plateau DEM with realistic features."""
    np.random.seed(seed)

    # Base elevation: 1200-1400m
    base_elevation = 1300.0

    # Large-scale terrain (ridges and valleys)
    x = np.linspace(0, 10, nx)
    y = np.linspace(0, 10, ny)
    X, Y = np.meshgrid(x, y)

    # Multi-scale Perlin-like noise
    z = np.zeros((ny, nx))
    for octave in range(4):
        freq = 2 ** octave
        amp = 100.0 / (2 ** octave)
        z += amp * np.sin(freq * X) * np.cos(freq * Y)

    # Add drainage pattern (SW to NE slope)
    z += 50.0 * (X + Y)

    # Add gully erosion features
    gully_mask = (np.abs(Y - 5.0) < 1.0) & (X > 3.0)
    z[gully_mask] -= 30.0

    return base_elevation + z
```

### 2.3 Domain Bounds

**Geographic Coordinates:**
- **SW Corner:** 36.5°N, 109.5°E
- **NE Corner:** 36.509°N, 109.514°E (approximately 1 km × 1 km at this latitude)

**Grid Coordinates:**
- **nx × ny:** 100 × 100
- **Cell Size:** 100 m × 100 m
- **Total Area:** 1 km²

---

## 3. Initial Conditions (t = 0)

### 3.1 Soil Moisture Profile

A 4-layer profile representing a degraded, dry state characteristic of pre-restoration Loess Plateau conditions:

| Layer | Depth Range | Thickness | Initial θ | θ_r | θ_s |
|-------|-------------|-----------|-----------|-----|-----|
| 0 | 0-10 cm | 10 cm | 0.08 | 0.04 | 0.42 |
| 1 | 10-30 cm | 20 cm | 0.12 | 0.04 | 0.42 |
| 2 | 30-60 cm | 30 cm | 0.15 | 0.04 | 0.42 |
| 3 | 60-100 cm | 40 cm | 0.20 | 0.04 | 0.42 |

**Canonical Initialization:**
```c
void initialize_soil_moisture_profile(HydState* hyd) {
    const float INITIAL_THETA[4] = {0.08f, 0.12f, 0.15f, 0.20f};

    for (uint32_t i = 0; i < hyd->num_cells; i++) {
        for (uint32_t layer = 0; layer < 4; layer++) {
            hyd->theta[i * 4 + layer] = INITIAL_THETA[layer];
        }
    }
}
```

### 3.2 Soil Organic Matter (SOM)

A uniform **0.8% SOM** (8 kg/m³), representing a heavily degraded state typical of the Loess Plateau before the Grain-to-Green restoration program.

**Canonical Initialization:**
```c
void initialize_som(RegState* reg) {
    const float INITIAL_SOM = 8.0f;  // kg/m³ (0.8%)

    for (uint32_t i = 0; i < reg->num_cells; i++) {
        reg->som[i] = INITIAL_SOM;
    }
}
```

### 3.3 Vegetation Cover

A uniform **15% cover** (0.15), representing sparse, degraded grassland, with minor procedural patchiness.

**Canonical Initialization with Patchiness:**
```python
def initialize_vegetation_cover(nx=100, ny=100, base_cover=0.15, seed=0x56454745):
    """Initialize vegetation with realistic patchiness."""
    np.random.seed(seed)

    # Base uniform cover
    V = np.full((ny, nx), base_cover, dtype=np.float64)

    # Add 5% random variation for patchiness
    noise = np.random.randn(ny, nx) * 0.025
    V += noise

    # Clamp to [0, 1]
    V = np.clip(V, 0.0, 1.0)

    return V
```

### 3.4 Hydraulic Parameters

Standard Van Genuchten parameters for Loess Soil are used, sourced from Li et al. (2003) and Cao et al. (2011).

**Locked Van Genuchten Parameters (v0.3.3):**

| Parameter | Value | Units | Source |
|-----------|-------|-------|--------|
| K_sat | 2.5×10⁻⁵ | m/s | Li et al. 2003 |
| θ_r | 0.04 | m³/m³ | Li et al. 2003 |
| θ_s | 0.42 | m³/m³ | Li et al. 2003 |
| α | 0.5 | m⁻¹ | Cao et al. 2011 |
| n | 1.5 | - | Cao et al. 2011 |

These values are now locked and must not change without a schema version bump.

---

## 4. Climate Forcing (1995-2005)

### 4.1 Climate Data Source

A synthetic, deterministic daily time-series is generated based on historical averages for the region, with **450 mm/yr average rainfall** concentrated in a summer monsoon season (June-September).

**Climate Statistics:**
- **Annual Precipitation:** 450 mm/yr (range: 350-550 mm/yr)
- **Mean Temperature:** 9.5°C (range: -10°C to 28°C)
- **PET (Potential ET):** 1,200 mm/yr
- **Monsoon Fraction:** 65% of annual precip in JJAS (June-July-Aug-Sept)

### 4.2 Data File Format

The data is stored in a canonical CSV file (`oracle/data/loess_plateau_climate_1995_2005.csv`) within the repository.

**CSV Format:**
```csv
date,precipitation_mm,temperature_c,pet_mm,relative_humidity
1995-01-01,0.0,-8.5,0.5,0.42
1995-01-02,0.0,-7.2,0.6,0.45
1995-01-03,2.3,-5.8,0.7,0.58
...
```

### 4.3 Climate Generator

**Canonical Climate Generator:**
```python
import pandas as pd
import numpy as np

def generate_loess_plateau_climate(start_date='1995-01-01', end_date='2005-01-01', seed=0x434C494D):
    """Generate synthetic climate time series for Loess Plateau."""
    np.random.seed(seed)

    dates = pd.date_range(start=start_date, end=end_date, freq='D')
    n_days = len(dates)

    # Day of year for seasonality
    doy = np.array([d.dayofyear for d in dates])

    # Temperature: sinusoidal with mean=9.5°C, amplitude=18.5°C
    T_mean = 9.5
    T_amp = 18.5
    temperature = T_mean + T_amp * np.sin(2 * np.pi * (doy - 80) / 365.25)
    temperature += np.random.randn(n_days) * 2.0  # Daily variability

    # Precipitation: concentrated in monsoon season
    precip = np.zeros(n_days)
    for i, d in enumerate(dates):
        if d.month in [6, 7, 8, 9]:  # Monsoon season
            # 65% of annual 450 mm over ~120 days = ~2.4 mm/day avg
            if np.random.rand() < 0.4:  # 40% of days have rain
                precip[i] = np.random.exponential(6.0)  # Exponential distribution
        else:
            # 35% over ~245 days = ~0.6 mm/day avg
            if np.random.rand() < 0.2:  # 20% of days have rain
                precip[i] = np.random.exponential(3.0)

    # PET: function of temperature (simplified Thornthwaite)
    pet = np.maximum(0.0, 0.5 + 0.08 * temperature)

    # Relative humidity
    rh = 0.5 + 0.2 * np.sin(2 * np.pi * (doy - 80) / 365.25)
    rh += np.random.randn(n_days) * 0.05
    rh = np.clip(rh, 0.2, 0.9)

    return pd.DataFrame({
        'date': dates,
        'precipitation_mm': precip,
        'temperature_c': temperature,
        'pet_mm': pet,
        'relative_humidity': rh
    })
```

---

## 5. Intervention Schedule

### 5.1 Baseline (No Intervention)

A control run with no interventions is performed to establish baseline degradation. This run serves as the reference for measuring intervention effectiveness.

**Expected Baseline Outcomes (10 years):**
- **Vegetation Cover:** 15% → 12% (further degradation)
- **SOM:** 0.8% → 0.6% (continued loss)
- **Total Runoff:** ~1,200 mm (poor infiltration)

### 5.2 Intervention Scenario

A scripted sequence of events simulates an aggressive restoration effort based on the real Grain-to-Green program:

**Intervention Timeline:**

| Year | Intervention | Location | Parameters |
|------|--------------|----------|------------|
| 1996 | Gravel Mulch | SW quadrant (x<50, y<50) | K_sat × 6.0 |
| 1997 | Swales | NW quadrant (x<50, y>50) | depression_storage + 0.5 m |
| 1998 | Check Dams | Main drainage (gully cells) | retention_capacity + 1.0 m |
| 2000 | Terracing | Eastern slopes (x>50, slope>10°) | slope reduction to 5° |
| 2002 | Tree Planting | Degraded patches (V<0.2) | V + 0.15, SOM + 5 kg/m³ |

**Canonical Intervention Application:**
```c
void apply_intervention_schedule(SimulationState* state, uint32_t year) {
    switch (year) {
        case 1996:
            for (uint32_t i = 0; i < state->num_cells; i++) {
                if (state->cells[i].x < 50 && state->cells[i].y < 50) {
                    apply_gravel_mulch(&state->cells[i]);
                }
            }
            break;

        case 1997:
            for (uint32_t i = 0; i < state->num_cells; i++) {
                if (state->cells[i].x < 50 && state->cells[i].y > 50) {
                    apply_swale(&state->cells[i]);
                }
            }
            break;

        case 1998:
            for (uint32_t i = 0; i < state->num_cells; i++) {
                if (state->cells[i].is_gully) {
                    apply_check_dam(&state->cells[i]);
                }
            }
            break;

        case 2000:
            for (uint32_t i = 0; i < state->num_cells; i++) {
                if (state->cells[i].x > 50 && state->cells[i].slope > 10.0f) {
                    apply_terracing(&state->cells[i]);
                }
            }
            break;

        case 2002:
            for (uint32_t i = 0; i < state->num_cells; i++) {
                if (state->cells[i].vegetation < 0.2f) {
                    apply_tree_planting(&state->cells[i]);
                }
            }
            break;
    }
}
```

### 5.3 Intervention Effects

**Locked Intervention Coefficients (v0.3.3):**

| Intervention | Parameter Modified | Multiplier/Offset |
|--------------|-------------------|-------------------|
| Gravel Mulch | K_sat | × 6.0 |
| Swale | depression_storage | + 0.5 m |
| Check Dam | retention_capacity | + 1.0 m |
| Terracing | slope | → 5° (clamped) |
| Tree Planting | vegetation | + 0.15 |
| Tree Planting | SOM | + 5.0 kg/m³ |

### 5.4 Locked Expected Outcomes (v0.3.3)

| Metric | Initial | Final (Restoration) | Change |
|--------|---------|---------------------|--------|
| Vegetation cover | 15% | 65–75% | +50–60% |
| SOM | 0.8% | 2.0–2.5% | +1.2–1.7% |
| Runoff (10 yr total) | ~1,200 mm | ~600 mm | –50% |

Validation passes only if the C simulation result is within **±5%** of these ranges.

---

## 6. Oracle Implementation (Python FP64)

### 6.1 Oracle Architecture

The Oracle is a reference implementation of the core physics solvers written in Python using NumPy `float64` for maximum precision. It is located in the `oracle/` directory of the repository and serves as the ground truth for the production C code.

**Oracle Directory Structure:**
```
oracle/
├── data/
│   └── loess_plateau_climate_1995_2005.csv
├── loess_plateau_oracle.py
├── hyd_rl_v1.py          # HYD-RLv1 solver
├── reg_v1.py             # REGv1 solver
├── reg_v2.py             # REGv2 solver
└── utils.py              # Shared utilities
```

### 6.2 Oracle Solver Implementation

**Example: HYD-RLv1 Oracle (Python):**
```python
import numpy as np

class HydRLv1Oracle:
    """High-precision FP64 reference implementation of HYD-RLv1."""

    def __init__(self, nx, ny, K_sat, theta_r, theta_s, alpha, n):
        self.nx = nx
        self.ny = ny
        self.K_sat = np.float64(K_sat)
        self.theta_r = np.float64(theta_r)
        self.theta_s = np.float64(theta_s)
        self.alpha = np.float64(alpha)
        self.n = np.float64(n)
        self.m = 1.0 - 1.0 / n

        # State: 4-layer soil moisture
        self.theta = np.zeros((ny, nx, 4), dtype=np.float64)

        # Initialize Van Genuchten LUT
        self.init_vg_lut()

    def init_vg_lut(self):
        """Initialize Van Genuchten lookup tables."""
        self.vg_lut_size = 256
        self.K_lut = np.zeros(self.vg_lut_size, dtype=np.float64)
        self.psi_lut = np.zeros(self.vg_lut_size, dtype=np.float64)

        for i in range(self.vg_lut_size):
            S_e = float(i) / (self.vg_lut_size - 1)

            # K(S_e) = K_sat · √S_e · [1 - (1 - S_e^(1/m))^m]²
            if S_e > 0:
                term1 = np.sqrt(S_e)
                term2 = 1.0 - (1.0 - S_e ** (1.0 / self.m)) ** self.m
                self.K_lut[i] = self.K_sat * term1 * term2 ** 2

            # ψ(S_e) = -1/α · (S_e^(-1/m) - 1)^(1/n)
            if S_e > 0:
                self.psi_lut[i] = -1.0 / self.alpha * (S_e ** (-1.0 / self.m) - 1.0) ** (1.0 / self.n)

    def step(self, precip, dt=3600.0):
        """Single timestep of Richards-Lite solver."""
        # ... (full implementation in oracle/hyd_rl_v1.py)
```

### 6.3 Oracle Execution

**Oracle Runner Script:**
```python
#!/usr/bin/env python3
"""Run the Loess Plateau Oracle simulation."""

import numpy as np
import pandas as pd
from loess_plateau_oracle import LoessPlateauOracle

def main():
    # Initialize oracle
    oracle = LoessPlateauOracle(nx=100, ny=100)

    # Load climate data
    climate = pd.read_csv('data/loess_plateau_climate_1995_2005.csv')

    # Run simulation
    print("Running Oracle (10 years, 3,653 days)...")
    hashes = []

    for day, row in climate.iterrows():
        # Run 24 HYD steps (1 per hour)
        for hour in range(24):
            oracle.hyd_step(precip=row['precipitation_mm'] / 24.0, dt=3600.0)

        # Compute daily state hash
        state_hash = oracle.compute_state_hash()
        hashes.append({'day': day, 'hash': state_hash})

        if day % 365 == 0:
            print(f"  Year {day // 365 + 1}/10 complete")

    # Save hashes
    pd.DataFrame(hashes).to_csv('oracle_hashes.csv', index=False)

    # Save final state
    np.savez('oracle_final_state.npz',
             theta=oracle.theta,
             vegetation=oracle.vegetation,
             som=oracle.som)

    print("Oracle run complete!")

if __name__ == '__main__':
    main()
```

---

## 7. Comparison Protocol

### 7.1 State Hash Comparison

The XXH64 hash of the entire simulation state is computed daily in both the C and Python runs. The **3,653 daily hashes must match exactly**.

**Canonical Hash Computation:**
```c
#include "xxhash.h"

uint64_t compute_state_hash(SimulationState* state) {
    XXH64_state_t* hash_state = XXH64_createState();
    XXH64_reset(hash_state, 0);

    // Hash all state arrays
    XXH64_update(hash_state, state->theta, state->num_cells * 4 * sizeof(float));
    XXH64_update(hash_state, state->vegetation, state->num_cells * sizeof(float));
    XXH64_update(hash_state, state->som, state->num_cells * sizeof(float));

    uint64_t hash = XXH64_digest(hash_state);
    XXH64_freeState(hash_state);

    return hash;
}
```

### 7.2 Field-Level Precision Check

The maximum absolute error between the final state fields of the Oracle and the C implementation must be less than **1×10⁻⁴**.

**Canonical Precision Comparison:**
```python
import numpy as np

def compare_precision(oracle_state, c_state):
    """Compare Oracle (FP64) vs C (FP32) final states."""

    fields = ['theta', 'vegetation', 'som', 'runoff']
    max_errors = {}

    for field in fields:
        oracle_field = oracle_state[field]
        c_field = c_state[field]

        # Compute absolute error
        abs_error = np.abs(oracle_field - c_field)
        max_error = np.max(abs_error)
        max_errors[field] = max_error

        # Check tolerance
        tolerance = 1e-4
        if max_error > tolerance:
            print(f"❌ FAIL: {field} max error {max_error:.2e} exceeds tolerance {tolerance:.2e}")
            return False
        else:
            print(f"✅ PASS: {field} max error {max_error:.2e}")

    return True
```

### 7.3 Mass Conservation Check

The total water budget (initial + precipitation - final - runoff - ET) must close with a relative error of less than **1×10⁻⁶**.

**Canonical Mass Balance:**
```python
def check_mass_balance(state, climate_data):
    """Verify water mass conservation."""

    # Initial water content
    initial_water = np.sum(state['theta_initial']) * 0.1  # m³ (10 cm layer depth)

    # Total precipitation
    total_precip = np.sum(climate_data['precipitation_mm']) / 1000.0  # m

    # Final water content
    final_water = np.sum(state['theta_final']) * 0.1  # m³

    # Total runoff
    total_runoff = np.sum(state['runoff']) / 1000.0  # m

    # Total ET
    total_et = np.sum(state['et']) / 1000.0  # m

    # Mass balance: initial + precip = final + runoff + ET
    input_water = initial_water + total_precip
    output_water = final_water + total_runoff + total_et

    balance_error = abs(input_water - output_water)
    relative_error = balance_error / input_water

    tolerance = 1e-6
    if relative_error > tolerance:
        print(f"❌ FAIL: Mass balance error {relative_error:.2e} exceeds tolerance {tolerance:.2e}")
        return False
    else:
        print(f"✅ PASS: Mass balance error {relative_error:.2e}")
        return True
```

---

## 8. Performance Benchmarks

The production C implementation must meet the following targets on the standard CI runner (GitHub Actions: Ubuntu 22.04, 2-core, 7 GB RAM):

| Metric | Target | Notes |
|--------|--------|-------|
| Simulation time (10 years) | < 60 s | Wall-clock time |
| Memory usage | < 500 MB | Peak RSS |
| Throughput | > 60 sim-years/hour | Real-time factor |
| HYD step latency | < 50 µs/cell | Average per cell |
| REG step latency | < 200 µs/cell | Average per cell |

**Canonical Performance Measurement:**
```c
#include <time.h>

void benchmark_simulation() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Run 10-year simulation
    run_loess_plateau_simulation();

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_s = (end.tv_sec - start.tv_sec) +
                       (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Simulation time: %.2f s\n", elapsed_s);

    // Check performance target
    const double TARGET_S = 60.0;
    if (elapsed_s > TARGET_S) {
        printf("❌ FAIL: Exceeded performance target (%.2f s > %.2f s)\n",
               elapsed_s, TARGET_S);
        exit(1);
    } else {
        printf("✅ PASS: Met performance target (%.2f s < %.2f s)\n",
               elapsed_s, TARGET_S);
    }
}
```

---

## 9. CI/CD Automation

### 9.1 GitHub Actions Workflow

The `.github/workflows/oracle_validation.yml` file defines the full automated workflow.

**Canonical Workflow:**
```yaml
name: Oracle Validation

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  oracle-validation:
    runs-on: ubuntu-22.04

    steps:
      - name: Checkout code
        uses: actions/checkout@v3

      - name: Set up Python 3.11
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'

      - name: Install Python dependencies
        run: |
          pip install numpy pandas xxhash

      - name: Run Python Oracle
        run: |
          cd oracle
          python3 loess_plateau_oracle.py
          echo "Oracle run complete"

      - name: Build C implementation
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j2

      - name: Run C implementation
        run: |
          ./build/negentropic_loess_plateau

      - name: Compare results
        run: |
          python3 oracle/compare_oracle_vs_c.py \
            --oracle-hashes oracle/oracle_hashes.csv \
            --c-hashes build/c_hashes.csv \
            --oracle-state oracle/oracle_final_state.npz \
            --c-state build/c_final_state.npz

      - name: Check performance
        run: |
          python3 oracle/check_performance.py \
            --log build/performance.log \
            --target-time-s 60

      - name: Upload artifacts
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: validation-failure-artifacts
          path: |
            oracle/oracle_hashes.csv
            build/c_hashes.csv
            oracle/oracle_final_state.npz
            build/c_final_state.npz
```

### 9.2 Comparison Script

**Canonical Comparison Script (`oracle/compare_oracle_vs_c.py`):**
```python
#!/usr/bin/env python3
"""Compare Oracle vs C implementation results."""

import sys
import argparse
import pandas as pd
import numpy as np

def compare_hashes(oracle_csv, c_csv):
    """Compare daily state hashes."""
    oracle_hashes = pd.read_csv(oracle_csv)
    c_hashes = pd.read_csv(c_csv)

    if len(oracle_hashes) != len(c_hashes):
        print(f"❌ FAIL: Hash count mismatch ({len(oracle_hashes)} vs {len(c_hashes)})")
        return False

    mismatches = []
    for i, (o_hash, c_hash) in enumerate(zip(oracle_hashes['hash'], c_hashes['hash'])):
        if o_hash != c_hash:
            mismatches.append(i)

    if mismatches:
        print(f"❌ FAIL: {len(mismatches)} hash mismatches on days: {mismatches[:10]}...")
        return False
    else:
        print(f"✅ PASS: All {len(oracle_hashes)} daily hashes match")
        return True

def compare_states(oracle_npz, c_npz):
    """Compare final states."""
    oracle = np.load(oracle_npz)
    c_impl = np.load(c_npz)

    all_pass = True
    for field in ['theta', 'vegetation', 'som']:
        max_error = np.max(np.abs(oracle[field] - c_impl[field]))
        tolerance = 1e-4

        if max_error > tolerance:
            print(f"❌ FAIL: {field} max error {max_error:.2e} > {tolerance:.2e}")
            all_pass = False
        else:
            print(f"✅ PASS: {field} max error {max_error:.2e}")

    return all_pass

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--oracle-hashes', required=True)
    parser.add_argument('--c-hashes', required=True)
    parser.add_argument('--oracle-state', required=True)
    parser.add_argument('--c-state', required=True)
    args = parser.parse_args()

    hash_pass = compare_hashes(args.oracle_hashes, args.c_hashes)
    state_pass = compare_states(args.oracle_state, args.c_state)

    if hash_pass and state_pass:
        print("\n✅ Oracle validation PASSED")
        sys.exit(0)
    else:
        print("\n❌ Oracle validation FAILED")
        sys.exit(1)

if __name__ == '__main__':
    main()
```

### 9.3 Failure Reporting

A failure at any step blocks the merge and notifies the developers with a detailed diff report including:
- List of days with hash mismatches
- Field-level error statistics
- Performance regression details
- Downloadable artifacts for local debugging

---

## 10. References

1. **Loess Plateau Restoration:** Li et al., "Water and sediment yield in response to land use change in the Loess Plateau" (2003)
2. **Van Genuchten Parameters:** Cao et al., "Soil hydraulic properties of the Loess Plateau" (2011)
3. **Oracle Validation:** Oberkampf & Roy, "Verification and Validation in Scientific Computing" (2010)
4. **CI/CD Best Practices:** Kim et al., "The DevOps Handbook" (2016)
5. **Numerical Precision:** Goldberg, "What Every Computer Scientist Should Know About Floating-Point Arithmetic" (1991)
6. **XXH64 Hashing:** Collet, Y. "xxHash - Extremely fast non-cryptographic hash algorithm" (2023)

---

## Document Control

- **Approved by:** Technical Lead
- **Review Frequency:** Every major release
- **Next Review:** v0.4.0
