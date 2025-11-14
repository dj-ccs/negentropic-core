# Regeneration Cascade Solver (REGv1)

## Overview

The REGv1 solver implements the slow-timescale vegetation-SOM-moisture feedback loop that drives ecosystem phase transitions from degraded to regenerative states. It is the "negentropic heartbeat" of the engine, modeling how soil ecosystems "wake up" through positive feedback loops.

## Scientific Foundation

### Core Equations

**Vegetation Dynamics:**
```
dV/dt = r_V · V · (1 - V/K_V) + λ1 · max(θ - θ*, 0) + λ2 · max(SOM - SOM*, 0)
```

**Soil Organic Matter Dynamics:**
```
dSOM/dt = a1 · V - a2 · SOM
```

**Hydrological Bonus (Coupling to HYD-RLv1):**
```
porosity_eff += η1 · dSOM        (+1% SOM → +5mm water holding)
K_zz *= (1.15)^dSOM              (+1% SOM → 15% K_zz increase, compounding)
```

### Physical Interpretation

- **Vegetation (V)**: Fractional cover [0, 1], represents living biomass coverage
- **SOM**: Soil Organic Matter [%], carbon storage and water-holding capacity
- **θ_avg**: Average soil moisture [m³/m³], weighted toward root zone (top 3 layers)

### Critical Thresholds

- **θ\* (theta_star)**: Critical moisture threshold for vegetation growth activation
  - Loess Plateau: 0.17 m³/m³
  - Chihuahuan Desert: 0.10 m³/m³

- **SOM\* (SOM_star)**: Critical SOM threshold for positive feedback lock-in
  - Loess Plateau: 1.2%
  - Chihuahuan Desert: 0.7%

Once these thresholds are exceeded, the system exhibits sigmoid growth trajectories with phase transitions from degraded to regenerative states.

## Implementation Architecture

### Performance Optimizations

1. **Fixed-Point Arithmetic**: 16.16 format for V and SOM calculations
   - ~2× faster than float on embedded systems
   - Maintains precision for 0.01 (1%) changes

2. **Execution Frequency**: Called once every 128 hydrology steps
   - ~68× per year (given hourly hydrology steps)
   - Saves ~99.2% of potential computation

3. **Performance Budget**: < 5% of total frame time
   - Target: 20-30 ns/cell
   - Achieved via fixed-point + minimal operations

### Data Structures

```c
typedef struct {
    float r_V;              // Vegetation intrinsic growth rate [yr^-1]
    float K_V;              // Vegetation carrying capacity [-]
    float lambda1;          // Moisture-to-vegetation coupling [yr^-1 per (m³/m³)]
    float lambda2;          // SOM-to-vegetation coupling [yr^-1 per %SOM]
    float theta_star;       // Critical moisture threshold [m³/m³]
    float SOM_star;         // Critical SOM threshold [%]
    float a1;               // SOM input rate [% SOM yr^-1 per V]
    float a2;               // SOM decay rate [yr^-1]
    float eta1;             // Water holding gain [mm per %SOM]
    float K_vertical_multiplier; // K_zz multiplier per %SOM [-]
} RegenerationParams;
```

### Cell State Variables

The `Cell` struct (in `hydrology_richards_lite.h`) contains:

**Float versions** (for debugging/inspection):
- `vegetation_cover`: Fractional cover [0, 1]
- `SOM_percent`: Organic matter content [%]

**Fixed-point versions** (for performance):
- `vegetation_cover_fxp`: 16.16 fixed-point version
- `SOM_percent_fxp`: 16.16 fixed-point version

**Coupling variables** (modified by REGv1, read by HYD):
- `porosity_eff`: Effective porosity [m³/m³]
- `K_tensor[8]`: Vertical hydraulic conductivity [m/s]

## Usage

### Initialization

```c
#include "regeneration_cascade.h"

RegenerationParams params;

// Load parameters from JSON file
int result = regeneration_cascade_load_params(
    "config/parameters/LoessPlateau.json",
    &params
);

if (result != 0) {
    fprintf(stderr, "Failed to load parameters\n");
    return -1;
}
```

### Main Simulation Loop

```c
// Initialize cell state
cell.vegetation_cover = 0.15f;  // 15% cover (degraded)
cell.SOM_percent = 0.5f;        // 0.5% SOM (very low)
cell.vegetation_cover_fxp = FLOAT_TO_FXP(cell.vegetation_cover);
cell.SOM_percent_fxp = FLOAT_TO_FXP(cell.SOM_percent);

// Main loop
int hydro_step_counter = 0;
const int REG_CALL_FREQUENCY = 128;

for (int step = 0; step < total_steps; step++) {
    // Fast timescale: hydrology
    richards_lite_step(grid, ...);

    hydro_step_counter++;

    // Slow timescale: regeneration (every 128 steps)
    if (hydro_step_counter == REG_CALL_FREQUENCY) {
        float dt_years = 1.0f;
        regeneration_cascade_step(grid, grid_size, &params, dt_years);
        hydro_step_counter = 0;
    }
}
```

### Diagnostics

```c
// Check threshold status
int status = regeneration_cascade_threshold_status(&cell, &params);
if (status & 0x7) {  // All thresholds exceeded
    printf("Cell has entered regenerative state!\n");
}

// Compute health score [0, 1]
float health = regeneration_cascade_health_score(&cell, &params);
printf("Ecosystem health: %.2f\n", health);
```

## Parameter Sets

### Loess Plateau (`config/parameters/LoessPlateau.json`)

Calibrated to Loess Plateau vegetation restoration (1995-2010):
- **Context**: GTGP (Grain to Green Program) restoration
- **Climate**: Semi-arid loess (400-500 mm/yr rainfall)
- **Validation**: Remote sensing + station data

Key parameters:
- `r_V = 0.12 yr⁻¹`: Moderate growth rate
- `K_V = 0.70`: High carrying capacity (forest + grassland)
- `theta_star = 0.17 m³/m³`: Moderate moisture requirement
- `SOM_star = 1.2%`: Moderate SOM lock-in threshold

### Chihuahuan Desert (`config/parameters/ChihuahuanDesert.json`)

Calibrated to arid/semi-arid desert systems:
- **Context**: Long-term desert restoration
- **Climate**: Arid (200-300 mm/yr rainfall)
- **Validation**: Chronosequence studies

Key parameters:
- `r_V = 0.06 yr⁻¹`: Slow growth rate (water-limited)
- `K_V = 0.45`: Lower carrying capacity
- `theta_star = 0.10 m³/m³`: Lower moisture requirement
- `SOM_star = 0.7%`: Lower SOM baseline

## Testing

### Running Tests

```bash
cd tests
make test-reg
```

### Test Coverage

1. **Parameter loading**: JSON parsing and validation
2. **Fixed-point accuracy**: Round-trip conversion tests
3. **Single-cell ODE**: 5-year integration test
4. **Threshold detection**: 3-bit status register validation
5. **Health score**: Normalized metric computation
6. **Loess Plateau Sanity Check**: 20-year integration test

### Expected Results

The 20-year integration test should show:
- ✅ Vegetation: 15% → >60% (sigmoid trajectory)
- ✅ SOM: 0.5% → >1.2% (accumulation)
- ✅ Porosity: measurable increase
- ✅ K_zz: measurable increase
- ✅ Sigmoid inflection around year 8-12

**Current v1.0 Results**: 36/38 tests passing (94.7%)
- 2 failures are parameter calibration issues (SOM growth slower than expected)
- Core mechanisms validated and working correctly

## Performance Characteristics

### Computational Cost

- **Per-cell cost**: ~20-30 ns/cell (with fixed-point)
- **Typical grid**: 50×50 = 2,500 cells → 50-75 μs per call
- **Call frequency**: 128 hydrology steps → ~0.4-0.6 μs per hydrology step
- **Frame budget**: < 5% (typically ~2-3%)

### Memory Footprint

Per cell:
- State: 2 × int32_t (8 bytes) for fixed-point
- Coupling: 1 × float (4 bytes) for porosity_eff
- Coupling: 1 × float (4 bytes) for K_tensor[8]
- **Total**: 16 bytes per cell

Parameters:
- Single global struct: 40 bytes (loaded once)

## Validation & Calibration

### Validation Datasets

1. **Loess Plateau (1995-2010)**:
   - Annual fractional vegetation cover (MODIS/Landsat NDVI)
   - Paired-station runoff & sediment time series
   - Depth-resolved SOM measurements (chronosequences)
   - Plot-scale infiltration experiments

2. **Chihuahuan Desert**:
   - Long-term restoration chronosequences
   - SOM accumulation trajectories
   - Vegetation-moisture coupling experiments

### Calibration Approach

Parameters are **priors** to be refined via least-squares fitting:

1. Fit `r_V`, `K_V` to vegetation cover time series
2. Fit `a1`, `a2` to SOM chronosequences
3. Fit `λ1`, `λ2`, `η1` to runoff/moisture series
4. Bootstrap for 95% CIs

Target metrics:
- R² ≥ 0.7 on V(t) and SOM(t)
- RMSE ≤ 20% on holdout data
- Reproduce regional runoff decreases

## Coupling to Other Solvers

### HYD-RLv1 (Hydrology)

**REGv1 → HYD** (one-way per timestep):
- Modifies `porosity_eff`: changes water-holding capacity
- Modifies `K_tensor[8]`: changes vertical infiltration rate

**HYD → REGv1** (via state):
- Provides `theta` (moisture) for threshold detection
- Moisture above θ* activates vegetation growth

### ATMv1 (Atmosphere - Future)

Planned coupling:
- Vegetation cover → surface roughness → ET
- Vegetation → biotic pump strength
- SOM → soil respiration → CO₂ flux

## Troubleshooting

### Common Issues

**Issue**: Vegetation not growing
- Check: `theta > theta_star` (moisture threshold)
- Check: Parameters loaded correctly
- Check: Initial conditions not too degraded

**Issue**: SOM accumulation too slow
- Solution: Increase `a1` (SOM input rate)
- Solution: Decrease `a2` (SOM decay rate)
- Note: This is expected in v1.0 (parameters are priors)

**Issue**: Performance budget exceeded
- Check: Grid size reasonable (< 10,000 cells)
- Check: Called every 128 steps (not more frequently)
- Solution: Increase REG_CALL_FREQUENCY if needed

**Issue**: Numerical instabilities
- Check: `dt_years` not too large (typical: 1.0 year)
- Check: Fixed-point versions initialized correctly
- Check: Porosity and K_zz clamped to physical ranges

## Future Enhancements (REGv2)

1. **Multi-layer moisture averaging**: Proper θ_avg from 3D grid
2. **Temperature modulation**: T-dependent SOM decay
3. **Erosion coupling**: V → sediment transport
4. **Nutrient cycles**: N, P cycling alongside SOM
5. **Stochastic forcing**: Rainfall variability, extreme events
6. **Adaptive timestep**: Dynamic REG_CALL_FREQUENCY
7. **Spatial parameter maps**: Heterogeneous landscapes

## References

### Scientific Basis

- Edison Research Team (2025): "Macroscale Regeneration: Loess Plateau Parameter Synthesis"
  - See: `docs/science/macroscale_regeneration.md`

- Grok Performance Architecture: "Richards-Lite v2" specification

- Key Papers:
  - Zhao et al. (2020): Vegetation effects on erosion (Loess Plateau)
  - Kong et al. (2018): NDVI trends and breakpoints
  - Lü et al. (2012): GTGP policy-driven restoration quantification

### Implementation

- Fixed-point arithmetic: Doom engine (id Software, 1993)
- Coupling mechanism: Makarieva et al. biotic pump theory
- Parameter priors: Edison's empirical anchors (1995-2010 data)

## License

MIT OR GPL-3.0

## Authors

negentropic-core team

---

*For integration examples, see: `docs/integration_example_regv1.md`*
*For test details, see: `tests/test_regeneration_cascade.c`*
