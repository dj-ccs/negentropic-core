# Atmospheric and Cloud System v0.3.3

**Version:** v0.3.3
**Status:** 🚧 Planned (awaiting ATMv1 + deck.gl integration)
**Last Updated:** 2025-11-16

## Purpose

This document defines the visual and physical coupling of the dynamic cloud layer in `negentropic-core`. It specifies the advection ODE for cloud particle movement, the GLSL shader layout for the custom deck.gl layer, noise domain warping, and coupling rules between cloud moisture and atmospheric humidity fields.

---

## 0. Current Status (from Architecture Quick Reference)

| Item | Status | Notes |
|------|--------|-------|
| Lagrangian Particle Advection | Spec complete | RK4, w_c + w_buoyancy |
| Moisture ODE | Spec complete | γ_cond, γ_evap, γ_precip·m² |
| Multi-octave Perlin + Domain Warping | Spec complete | 4 octaves, deterministic RNG seeding |
| deck.gl Custom Layer | Spec complete | Billboard, radial falloff, moisture color, age fade |
| Condensation / Evaporation / Precipitation coupling | Spec complete | 5 %/s evap, quadratic autoconversion |
| Biotic Pump (optional) | Spec complete | 1 + 2.0·V, continuity threshold |
| Full ATMv1 integration | 🚧 Planned | Awaiting atmospheric solver |
| Performance | 60 FPS @ 100k particles (target) | TBD |

---

## 1. Overview

### 1.1 Dual-Layer Architecture

1. **Core Atmospheric State (Grid-Based):** Temperature (θ), Humidity (q), Pressure (p), Wind (u, v).
2. **Cloud Particles (Lagrangian):** Position (x, y, z), Moisture (m), Lifetime (τ). Rendered via a deck.gl custom layer.

### 1.2 Coupling Strategy

- **Atmospheric → Clouds:** Wind advects particles; humidity condenses into cloud moisture.
- **Clouds → Atmospheric:** Precipitation removes cloud moisture; shading modifies surface temperature.

---

## 2. Cloud Particle Advection ODE

### 2.1 Governing Equation

**Position Evolution:**
`dx/dt = u(x, y, z, t) + w_turb(x, y, z, t)`
`dy/dt = v(x, y, z, t) + w_turb(x, y, z, t)`
`dz/dt = w_c(x, y, t) + w_buoyancy(m, T)`

Where `u, v` are from the atmospheric solver, `w_c` is from the Torsion Module, and `w_turb` is from a domain-warped Perlin noise field.

**Moisture Evolution:**
`dm/dt = γ_cond · max(q - q_sat, 0) - γ_evap · m - γ_precip · m²`

---

## 3. Discrete Integration Scheme

A fourth-order Runge-Kutta (RK4) integrator is mandated for smooth, accurate particle trajectories. Velocity and atmospheric state values are sampled from the grid using bilinear interpolation.

**Canonical RK4 Implementation:**
```c
void integrate_particle_rk4(CloudParticle* p, AtmosphericState* atm, float dt) {
    // k1 = f(t, x)
    vec3 k1_pos = compute_velocity(p->pos, atm);
    float k1_moisture = compute_dmdt(p->moisture, atm, p->pos);

    // k2 = f(t + dt/2, x + k1*dt/2)
    vec3 pos_k2 = vec3_add(p->pos, vec3_scale(k1_pos, dt/2));
    vec3 k2_pos = compute_velocity(pos_k2, atm);
    float k2_moisture = compute_dmdt(p->moisture + k1_moisture*dt/2, atm, pos_k2);

    // k3 = f(t + dt/2, x + k2*dt/2)
    vec3 pos_k3 = vec3_add(p->pos, vec3_scale(k2_pos, dt/2));
    vec3 k3_pos = compute_velocity(pos_k3, atm);
    float k3_moisture = compute_dmdt(p->moisture + k2_moisture*dt/2, atm, pos_k3);

    // k4 = f(t + dt, x + k3*dt)
    vec3 pos_k4 = vec3_add(p->pos, vec3_scale(k3_pos, dt));
    vec3 k4_pos = compute_velocity(pos_k4, atm);
    float k4_moisture = compute_dmdt(p->moisture + k3_moisture*dt, atm, pos_k4);

    // x(t + dt) = x(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
    p->pos = vec3_add(p->pos, vec3_scale(
        vec3_add(vec3_add(k1_pos, vec3_scale(k2_pos, 2)),
                 vec3_add(vec3_scale(k3_pos, 2), k4_pos)), dt/6));

    p->moisture += (dt/6) * (k1_moisture + 2*k2_moisture + 2*k3_moisture + k4_moisture);
}
```

---

## 4. Noise Domain Warping

To add realistic turbulence and break visual repetition, a multi-octave (N=4), domain-warped Perlin noise field `w_turb` is added to the particle advection equation. The Perlin noise generator is initialized deterministically from the core `NegRNG` to ensure reproducibility.

**Warped Noise:**
`P_warped(x, y, z) = P(x + P(x+...), y + P(y+...), z + P(z+...))`

**Canonical Implementation:**
```c
float domain_warped_noise(vec3 pos, NegRNG* rng) {
    // First layer of noise for warping
    float offset_x = perlin_noise_3d(pos.x + 100.0f, pos.y, pos.z, rng);
    float offset_y = perlin_noise_3d(pos.x, pos.y + 200.0f, pos.z, rng);
    float offset_z = perlin_noise_3d(pos.x, pos.y, pos.z + 300.0f, rng);

    // Warped position
    vec3 warped_pos = {
        pos.x + offset_x * 50.0f,
        pos.y + offset_y * 50.0f,
        pos.z + offset_z * 50.0f
    };

    // Multi-octave Perlin noise at warped position
    float result = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    for (int i = 0; i < 4; i++) {
        result += amplitude * perlin_noise_3d(
            warped_pos.x * frequency,
            warped_pos.y * frequency,
            warped_pos.z * frequency,
            rng
        );
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return result;
}
```

---

## 5. deck.gl Custom Cloud Layer

### 5.1 Layer Architecture

A custom `CloudParticleLayer` extends the base `deck.gl/core Layer` class. It uses an instanced `luma.gl/engine Model` with a billboard geometry to render each particle.

**TypeScript Layer Structure:**
```typescript
export class CloudParticleLayer extends Layer {
    static layerName = 'CloudParticleLayer';

    getShaders() {
        return {
            vs: cloudVertexShader,
            fs: cloudFragmentShader,
            modules: [project32, picking]
        };
    }

    initializeState() {
        const {gl} = this.context;
        const model = new Model(gl, {
            id: `${this.props.id}-cloud-particles`,
            vs: this.getShaders().vs,
            fs: this.getShaders().fs,
            geometry: new Geometry({
                drawMode: GL.TRIANGLE_FAN,
                vertexCount: 4,
                attributes: {
                    positions: new Float32Array([-1,-1, 1,-1, 1,1, -1,1])
                }
            }),
            isInstanced: true,
            instanceCount: this.props.data.length
        });
        this.setState({model});
    }
}
```

### 5.2 Vertex Shader (GLSL)

The vertex shader implements billboarding to ensure particles always face the camera. It transforms the particle's ECEF position and passes its moisture and age to the fragment shader.

**Canonical Vertex Shader:**
```glsl
#version 300 es
#define SHADER_NAME cloud-particle-vertex-shader

uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
uniform vec3 uCameraPosition;

in vec2 positions;  // Billboard quad corners
in vec3 instancePositions;  // Particle ECEF position
in float instanceMoisture;
in float instanceAge;

out float vMoisture;
out float vAge;
out vec2 vUV;

void main() {
    vMoisture = instanceMoisture;
    vAge = instanceAge;
    vUV = positions * 0.5 + 0.5;

    // Billboard: construct basis facing camera
    vec3 toCamera = normalize(uCameraPosition - instancePositions);
    vec3 right = normalize(cross(vec3(0.0, 0.0, 1.0), toCamera));
    vec3 up = cross(toCamera, right);

    // Scale based on moisture (larger particles = more moisture)
    float size = 50.0 + instanceMoisture * 200.0;  // 50-250m radius

    // Construct billboarded position
    vec3 worldPos = instancePositions
                  + right * positions.x * size
                  + up * positions.y * size;

    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(worldPos, 1.0);
}
```

### 5.3 Fragment Shader (GLSL)

The fragment shader renders each particle as a soft, circular sprite with properties driven by its state:
- **Shape:** A radial falloff (`1.0 - smoothstep(0.5, 1.0, dist)`) creates a soft-edged appearance.
- **Color:** Brightness is based on moisture content (`vMoisture`), ranging from dark gray to bright white.
- **Texture:** A simple procedural noise function adds visual detail.
- **Opacity:** Final alpha is a product of the radial falloff, base opacity, and an age-based fade (`1.0 - clamp(vAge / 3600.0, 0.0, 1.0)`).

**Canonical Fragment Shader:**
```glsl
#version 300 es
#define SHADER_NAME cloud-particle-fragment-shader
precision highp float;

in float vMoisture;
in float vAge;
in vec2 vUV;

out vec4 fragColor;

// Simple hash-based noise for texture detail
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

void main() {
    // Radial distance from particle center
    float dist = length(vUV - 0.5) * 2.0;

    // Soft circular falloff
    float radialAlpha = 1.0 - smoothstep(0.5, 1.0, dist);

    // Moisture-based brightness (0.1-1.0 range, normalized)
    float brightness = 0.3 + 0.7 * clamp(vMoisture / 10.0, 0.0, 1.0);

    // Procedural texture detail
    float detail = noise(vUV * 8.0) * 0.2 + 0.8;

    // Age-based fade (particles fade after 1 hour)
    float ageFade = 1.0 - clamp(vAge / 3600.0, 0.0, 1.0);

    // Final color: white-ish cloud with subtle detail
    vec3 color = vec3(brightness * detail);

    // Final alpha: radial * base opacity * age fade
    float alpha = radialAlpha * 0.6 * ageFade;

    fragColor = vec4(color, alpha);
}
```

---

## 6. Coupling with Atmospheric Humidity

### 6.1 Condensation (Atmospheric → Clouds)

In grid cells where humidity `q` exceeds the saturation humidity `q_sat`, the excess moisture is converted into new cloud particles.

**Saturation Humidity (Clausius-Clapeyron):**
`q_sat(T) = 0.622 · e_sat(T) / (p - e_sat(T))`
`e_sat(T) = 611 · exp(17.67 · (T - 273.15) / (T - 29.65))`

**Condensation Rule:**
```c
void atmospheric_condensation_step(AtmosphericCell* cell, CloudState* clouds, float dt) {
    float q_sat = compute_saturation_humidity(cell->temperature, cell->pressure);
    float excess = cell->humidity - q_sat;

    if (excess > 0.0f) {
        // Create new cloud particle
        CloudParticle new_particle = {
            .pos = {cell->x, cell->y, cell->z + 1000.0f},  // 1 km altitude
            .moisture = excess * GAMMA_COND * dt,
            .age = 0.0f
        };
        add_cloud_particle(clouds, new_particle);

        // Remove moisture from atmospheric grid
        cell->humidity -= new_particle.moisture;
    }
}
```

### 6.2 Evaporation (Clouds → Atmospheric)

Cloud particles lose moisture back to the atmospheric grid at a fixed rate (`γ_evap`). This moisture is added to the humidity `q` of the grid cell containing the particle.

**Evaporation Rule:**
```c
void cloud_evaporation_step(CloudParticle* p, AtmosphericState* atm, float dt) {
    float dm_evap = GAMMA_EVAP * p->moisture * dt;
    p->moisture -= dm_evap;

    // Add moisture back to atmospheric grid cell at particle location
    AtmosphericCell* cell = get_cell_at_position(atm, p->pos);
    cell->humidity += dm_evap;

    // Remove particle if depleted
    if (p->moisture < 0.01f) {
        p->is_active = false;
    }
}
```

### 6.3 Precipitation (Clouds → Surface)

When a particle's moisture content `m` becomes large, it begins to precipitate at a rate quadratic in `m` (`γ_precip · m²`). This removes moisture from the particle and adds it to the surface `precipitation` field for the HYD-RLv1 solver.

**Precipitation Rule (Quadratic Autoconversion):**
```c
void cloud_precipitation_step(CloudParticle* p, SurfaceGrid* surface, float dt) {
    float dm_precip = GAMMA_PRECIP * p->moisture * p->moisture * dt;
    p->moisture -= dm_precip;

    // Add to surface precipitation field
    SurfaceCell* cell = get_surface_cell_at_xy(surface, p->pos.x, p->pos.y);
    cell->precipitation += dm_precip;  // mm/hr
}
```

---

## 7. Biotic Pump Coupling (Optional)

### 7.1 Enhanced Condensation Over Vegetation

When enabled, the condensation rate is enhanced over vegetated areas, simulating the biotic pump's effect on moisture convergence.
`γ_cond_eff = γ_cond · (1 + β_veg · V)`

**Canonical Implementation:**
```c
float compute_effective_condensation_rate(float vegetation_cover) {
    return GAMMA_COND * (1.0f + BETA_VEG * vegetation_cover);
}
```

### 7.2 Forest Continuity Thresholds

The biotic pump mechanism is only activated when vegetation cover `V` exceeds a threshold (e.g., `V > 0.6`) over a contiguous area greater than a minimum size (e.g., 100 km²), checked via a breadth-first search.

**Canonical BFS Check:**
```c
bool check_forest_continuity(SurfaceGrid* grid, int start_x, int start_y) {
    if (grid->cells[start_y][start_x].vegetation < 0.6f) return false;

    // BFS to find contiguous vegetated area
    int visited[MAX_GRID_SIZE][MAX_GRID_SIZE] = {0};
    Queue queue;
    queue_init(&queue);
    queue_push(&queue, (Coord){start_x, start_y});

    float total_area = 0.0f;
    while (!queue_empty(&queue)) {
        Coord c = queue_pop(&queue);
        if (visited[c.y][c.x]) continue;
        visited[c.y][c.x] = 1;

        if (grid->cells[c.y][c.x].vegetation < 0.6f) continue;
        total_area += grid->cell_area_km2;

        // Add neighbors
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                queue_push(&queue, (Coord){c.x + dx, c.y + dy});
            }
        }
    }

    return total_area >= 100.0f;  // 100 km² threshold
}
```

### 7.3 Locked Coupling Coefficients (v0.3.3)

| Coefficient | Value | Meaning | Source |
|-------------|-------|---------|--------|
| γ_cond | 0.1 s⁻¹ | Base condensation rate | Quick-ref |
| γ_evap | 0.05 s⁻¹ | Evaporation rate (5 %/s) | Quick-ref |
| γ_precip | 0.02 | Quadratic precipitation coefficient | Quick-ref |
| β_veg | 2.0 | Biotic pump vegetation multiplier | Quick-ref |

These values are now locked and must not change without a schema version bump.

---

## 8. Performance Targets

| Operation | Target |
|-----------|--------|
| Particle integration (RK4) | < 200 ns/particle |
| Noise sampling | < 50 ns/sample |
| Grid coupling | < 10 ns/particle |
| deck.gl rendering | 60 FPS @ 100k particles |
| **Total overhead** | **< 20% of frame budget** |

---

## 9. Data Structures

**`CloudParticle` Structure:**
```c
typedef struct {
    vec3 pos;           // ECEF position (m)
    float moisture;     // Water content (kg/m³)
    float age;          // Lifetime (seconds)
    bool is_active;     // Active flag
} CloudParticle;
```

**`CloudState` Structure:**
```c
typedef struct {
    CloudParticle* particles;   // Dynamic array
    uint32_t num_particles;
    uint32_t capacity;
    NegRNG rng;                 // For turbulence noise
} CloudState;
```

---

## 10. Validation

Validation will be performed via synthetic test cases (e.g., uniform wind, static atmosphere) and visual quality metrics assessing spatial coverage, temporal coherence, and realism.

**Validation Test Cases:**
1. **Uniform Wind:** All particles should advect uniformly in the wind direction with minimal dispersion.
2. **Static Atmosphere:** In a zero-wind scenario with constant humidity, cloud distribution should remain stable.
3. **Condensation Cycle:** A supersaturated cell should spawn particles, which should evaporate and return moisture to the grid.
4. **Biotic Pump:** Over a large forest (V > 0.6, area > 100 km²), condensation rate should be enhanced by 3× (1 + 2.0·1.0).

---

## 11. References

1. **Clausius-Clapeyron:** Bolton, D. "The computation of equivalent potential temperature" (1980)
2. **Biotic Pump:** Makarieva & Gorshkov, "Biotic pump of atmospheric moisture" (2007)
3. **Domain Warping:** Quilez, I. "Warp noise" (2014) https://iquilezles.org/articles/warp/
4. **Perlin Noise:** Perlin, K. "Improving noise" (2002)
5. **Cloud Microphysics:** Pruppacher & Klett, "Microphysics of Clouds and Precipitation" (2010)
6. **RK4 Integration:** Press et al., "Numerical Recipes" (2007), Chapter 17.1

---

## Document Control

- **Approved by:** Technical Lead
- **Review Frequency:** Every major release
- **Next Review:** v0.4.0
