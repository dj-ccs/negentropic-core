# Microscale Hydrology: Generalized Richards Equation with Earthwork Interventions

**Scientific Foundation for HYD-RLv1 Solver**

**Author:** Edison Scientific Research Team
**Date:** 2025-11-14
**Status:** Approved for Implementation

---

## Executive Summary

We provide implementable closures that embed microscale earthworks into a generalized Richards framework with explicit surface–subsurface coupling, fill‑and‑spill connectivity, and intervention‑specific conductivity/porosity modifiers. We quantify swale performance, gravel/rock mulch effects on runoff and evaporation, and microtopography thresholds, and deliver pseudocode, parameter JSON, and canonical tests. Where Loess Plateau (LP) dynamics and thresholds are requested, we provide partial placeholders pending additional evidence ingestion (flagged below). ([1.1], [1.2], [2.1], [3.1], [3.2], [4.1], [4.2], [5.1])

---

## 1. Mathematical Formulations (LaTeX)

### 1.1 Generalized Surface–Subsurface Flow with Intervention I

We adopt a unified Richards‑type form with a thin runoff layer ℓ_r at the surface, enforcing pressure/flux continuity and representing overland flow as a Darcy‑like diffusive wave. Domain indicator χ ∈ {runoff layer, vadose, saturated} selects parameter sets. ([1.1], [1.2])

```
∂θ/∂t = ∇·(K_eff(θ, I, ζ) ∇(ψ + z)) + S_I(x,y,t)

K_eff(θ, I, ζ) = K_mat(θ) · M_I(θ,s; slope,w,d) · C(ζ)
                  \_____intervention multiplier_____/   \connectivity/
```

where:
- θ is water content
- ψ matric head
- z elevation head
- ζ surface depression storage
- S_I encodes earthwork inflow/outflow (e.g., curb cuts, check dams)

The intervention multiplier M_I modifies magnitude and anisotropy of K and storage via effective porosity; C(ζ) captures fill‑and‑spill connectivity of microtopography. ([1.1], [2.1])

**Runoff Layer** (thickness ℓ_r) uses an equivalent porous film with hydraulic conductivity K_r(θ) and porosity ϕ_r; overland discharge q_s emerges from the same gradient:

```
q_s = -K_r(θ) ∇(η_s),    where η_s = h_s + z,    h_s ≡ surface water depth    [Eq 1]
```

This realizes infiltration‑excess and saturation‑excess runoff within one PDE by parameter continuity. ([1.1], [1.2])

### 1.2 Microtopography Fill‑and‑Spill Connectivity and Transmissivity Feedback

Following thresholded surface storage behavior, we close ([2.1]):

```
C(ζ) = 1/(1 + exp[-a_c(ζ - ζ_c)]),    ζ_c (m) = depression storage threshold

T(η) = T_0 exp(b_T (η - η_0)),    η ≡ water table head
```

so that lateral transmissivity increases nonlinearly as the water table rises (matrix–macropore activation). These terms can be embedded by making K_eff increase with η via T(η). ([2.1])

### 1.3 Intervention‑Specific Conductivity and Storage Modifiers

Let baseline van Genuchten–Mualem parameters be {K_s, α, n, ϕ, θ_s, θ_r}. Interventions alter these as:

```
K_s,eff(I) = K_s M_I^K

K_eff = R_I diag(M_I,xx^K, M_I,yy^K, M_I,zz^K) R_I^T K_mat(θ)

φ_eff(I) = φ + Δφ_I
θ_s,eff = θ_s + Δθ_s,I

α_eff(I) = α (1 + Δα_I)
n_eff = n (1 + Δn_I)
```

where R_I rotates anisotropy to align with earthwork orientation (e.g., along‑swale channeling). ([1.1], [5.1])

### 1.4 Evaporation Suppression and Energy Coupling (Mulches)

Implement an evaporation sink E_surf with a shading/cooling factor κ_I ≤ 1 (gravel/rock mulch):

```
E_mulch = κ_I E_bare,    κ_I ∈ [0.2, 0.6]    (gravel–sand mulch, semiarid loess)    [Eq 2]
```

Empirical ranges (Section 2.1) reduce cumulative evaporation by ≈4× vs bare over 14 d; choose κ_I ≈ 0.25–0.5 as calibrated by site. ([4.1], [4.2])

### 1.5 Source Terms for Earthworks S_I(x,y)

Represent distributed inflow/outflow as spatially varying source terms:

```
S_I = Σ_j q_j δ_Γ_j(x,y) - ∇·(χ_runoff q_s)    [Eq 3]

where:
- q_j = curb‐cut inflow
- Γ_j = edge set
```

with χ_runoff an indicator for the surface film domain and q_s from Eq 1. ([1.1])

### 1.6 Swale Hydraulics and Infiltration Partition

Side slopes and channel modeled as three parallel 1‑D elements exchanging with subsurface; wetted fraction f_w accounts for rill concentration rather than sheet flow ([3.1], [3.2], [5.1]):

```
I_event = f_w K_s,eff A_contact τ_contact,    f_w ∈ [0.1, 0.7]    [Eq 4]
```

Event classes: smallest ~40% fully infiltrated, next ~40% partially reduced, largest ~20% little attenuation—used for parameterizing τ_contact vs rainfall percentile. ([3.1], [3.2])

---

## 2. Quantified Effects of Earthworks (Equations + Ranges)

### 2.1 Gravel/Rock Mulch (Loess Region Experiments)

- **Runoff reduction**: Across 91 events, bare plots produced 48.4 mm total runoff (18 events) vs 3.4 mm (6 events) with gravel–sand mulch → event runoff fraction reduction factor R_M ≈ 0.07 (dimensionless). Implement as reduced overland K_r (or increased depression storage) and enhanced near‑surface K_s: M^K_mulch,zz ≳ 1, K_r → η K_r with η ≈ 0.1–0.3. ([4.1], [4.2])

- **Infiltration depth multiplier** during storms: d_inf,mulch/d_inf,bare ≈ 6 (≈60 cm vs 10 cm over 8–12 h). Use M^K_mulch,zz ≈ 2–6 as a calibration starting range. ([4.1], [4.2])

- **Evaporation suppression**: 14‑day evaporation bare/mulch ≈ 4 → κ_I ≈ 0.25 in Eq 2. ([4.1], [4.2])

- **Rainfall interception by mulch**: Per‑storm 0.48–0.74 mm (11–17%); implement as added depression storage Δζ ≈ 0.5–0.7 mm per event (scale with cover fraction). ([4.1], [4.2])

### 2.2 Swales / Grassed Ditches

- **Annual infiltration performance**: 9–100% reported; median/means ≈ 52–64% for volume reduction, with 0.56 cm initial abstraction and complete capture around K_s ≈ 1.5 cm h⁻¹ for suitable designs. Encode an infiltration multiplier M^K_swale,zz based on slope S_0, width w, and depth d: ([3.1], [3.2])

```
M^K_swale,zz = c_0 (1 + c_s tan(β) + c_w (w/w_0) + c_d (d/d_0)) f_w

where:
- β = side-slope angle
- f_w ∈ [0.1, 0.7]
- c_0 ~ 1
- c_{s,w,d} ~ O(0.1–0.5)
```

Use three parallel elements (two slopes + channel) with distinct f_w. ([3.1], [3.2])

### 2.3 Microtopography / Contour Berms / Check Dams

- **Connectivity function** C(ζ) with threshold ζ_c (mm–cm) from site microtopography; typical fill‑and‑spill switching yields subsurface‑dominated hydrographs most of the year with brief surface‑dominated pulses at ζ > ζ_c. ([2.1])

- **Check dams**: Represent as localized S_I inflow sinks converting runoff to storage upstream (increase Δζ, decrease K_r downstream reach), and progressive Δϕ_I > 0 in impoundment cells due to deposition (geomorphic feedback). ([2.1])

---

## 3. Soil‑Moisture Retention Curves for Surface States (Modified van Genuchten)

For each surface condition X ∈ {biocrust, bare degraded, mulched, OM‑enriched}, parameter shifts relative to baseline (site‑specific):

```
θ_s^(X) = θ_s + Δθ_s,X
φ^(X) = φ + Δφ_X

α^(X) = α (1 + Δα_X)
n^(X) = n (1 + Δn_X)

K_s^(X) = K_s M_X^K
```

**Empirical anchors**: Mulched vs bare shows strong evaporation suppression and deeper infiltration (choose M^K_mulched,zz ≈ 2–6, Δθ_s ≥ 0 from protected surface, Δα < 0 to reflect reduced macropore air entry). Biocrust often reduces infiltration (M^K < 1) but increases surface storage; encode via Δζ > 0 and M^K_zz < 1. Site calibration required; placeholders delivered as ranges in JSON below. ([4.1], [4.2], [2.1])

---

## 4. Vegetation–SOM–θ Coupled ODEs (Partial, Structure Ready for Parameterization)

We provide implementable forms; empirical LP parameter ranges are pending further evidence ingestion.

```
dV/dt = r_V V(1 - V/K_V) + λ_1 (θ - θ*)_+ + λ_2 (SOM - SOM*)_+

dSOM/dt = a_1 V - a_2 exp(γ_T(T)) g(θ) SOM

dθ/dt = ∇·(K_eff(θ, I, ζ) ∇(ψ+z)) + η_0 + η_1 SOM
```

where (x)_+ = max(x,0). The term η_1 captures water‑holding gains from organic matter. Stability and thresholds follow from transcritical/saddle‑node bifurcations in V with moisture/SOM subsidies. ([2.1], [1.1])

---

## 5. Parameter Tables (JSON; Ranges Grounded in Retrieved Field Studies)

```json
{
  "Hydraulic": {
    "K_s": {
      "units": "m s^-1",
      "site_range": [1e-7, 1e-4],
      "notes": "Baseline saturated conductivity (soil-specific)",
      "sources": ["[1.1]"]
    },
    "M_mulch^K": {
      "units": "-",
      "range": [2, 6],
      "notes": "Vertical Ks multiplier under gravel–sand mulch (deeper infiltration)",
      "sources": ["[4.1]", "[4.2]"]
    },
    "kappa_mulch": {
      "units": "-",
      "range": [0.25, 0.5],
      "notes": "Evaporation suppression factor E_mulch = kappa*E_bare",
      "sources": ["[4.1]", "[4.2]"]
    },
    "R_runoff_mulch": {
      "units": "-",
      "range": [0.05, 0.15],
      "notes": "Runoff fraction vs bare; ~7% observed in Loess plots",
      "sources": ["[4.1]", "[4.2]"]
    },
    "zeta_intercept_mulch": {
      "units": "mm per event",
      "range": [0.5, 0.8],
      "notes": "Rainfall interception by pebble/gravel mulch",
      "sources": ["[4.1]", "[4.2]"]
    },
    "M_swale^K": {
      "units": "-",
      "range": [1, 3],
      "notes": "Effective Ks multiplier (vertical) for swale contact zone; depends on slope/width/depth & wetted fraction",
      "sources": ["[3.1]", "[3.2]"]
    },
    "f_w": {
      "units": "-",
      "range": [0.1, 0.7],
      "notes": "Wetted surface fraction due to rill concentration",
      "sources": ["[3.1]", "[3.2]", "[5.1]"]
    },
    "K_r": {
      "units": "m s^-1",
      "range": [1e-6, 1e-3],
      "notes": "Runoff layer conductivity (diffusive wave as Darcy film)",
      "sources": ["[1.1]"]
    },
    "zeta_c": {
      "units": "mm",
      "range": [2, 20],
      "notes": "Depression storage threshold for fill-and-spill connectivity",
      "sources": ["[2.1]"]
    },
    "a_c": {
      "units": "mm^-1",
      "range": [0.2, 2.0],
      "notes": "Steepness of connectivity function C(zeta)",
      "sources": ["[2.1]"]
    },
    "b_T": {
      "units": "m^-1",
      "range": [0.5, 3.0],
      "notes": "Transmissivity feedback coefficient T(eta) = T0 exp(b_T (eta-eta0))",
      "sources": ["[2.1]"]
    }
  },
  "RetentionCurveShifts": {
    "Delta_theta_s_mulch": {
      "units": "-",
      "range": [0.00, 0.05],
      "notes": "Surface protection increases effective saturation capacity modestly",
      "sources": ["[4.1]", "[4.2]"]
    },
    "Delta_alpha_mulch": {
      "units": "-",
      "range": [-0.3, 0.0],
      "notes": "Reduced air entry (less crusting) under mulch",
      "sources": ["[4.1]", "[4.2]"]
    },
    "Delta_n_mulch": {
      "units": "-",
      "range": [-0.1, 0.1],
      "notes": "Structure changes small; calibrate site-specific",
      "sources": ["[4.1]", "[4.2]"]
    }
  },
  "SwalePerformance": {
    "IA_threshold": {
      "units": "cm",
      "range": [0.5, 0.7],
      "notes": "Initial abstraction for full capture threshold",
      "sources": ["[3.1]", "[3.2]"]
    },
    "Ks_full_capture": {
      "units": "cm h^-1",
      "range": [1.2, 1.8],
      "notes": "Saturated Ks for complete capture (design-specific)",
      "sources": ["[3.1]", "[3.2]"]
    },
    "volume_reduction": {
      "units": "%",
      "range": [50, 70],
      "notes": "Typical annual reduction range across studies",
      "sources": ["[3.1]", "[3.2]"]
    }
  },
  "SOM_Veg_Coupling": {
    "r_V": {
      "units": "yr^-1",
      "range": [0.05, 0.3],
      "notes": "Intrinsic vegetation growth rate (placeholder – LP calibration pending)",
      "sources": []
    },
    "a1": {
      "units": "kg C m^-2 yr^-1 per V",
      "range": [0.1, 0.5],
      "notes": "SOM input coefficient (placeholder)",
      "sources": []
    },
    "a2": {
      "units": "yr^-1",
      "range": [0.05, 0.2],
      "notes": "SOM decay coefficient (placeholder)",
      "sources": []
    },
    "eta1": {
      "units": "-",
      "range": [0.01, 0.1],
      "notes": "Water-holding increment per unit SOM (placeholder)",
      "sources": []
    }
  }
}
```

---

## 6. Solver Pseudocode (Mixed Surface–Subsurface with Interventions)

**Core PDE**: ∂θ/∂t = ∇·(K_eff(θ,I,ζ) ∇(ψ+z)) + S_I; runoff layer film coupled at the surface.

**Algorithm** (implicit in time; Picard nonlinearity; mixed FEM or finite volume): ([1.1], [1.2])

**Inputs**: mesh; van Genuchten params; intervention maps I(x,y); runoff‑layer ℓ_r, K_r(θ); microtopography fields ζ_c(x,y), a_c; connectivity C(ζ); swale elements/geometry (slope β, width w, depth d, wetted fraction f_w); rainfall/runon time series; evaporation factors κ_I.

**For each timestep n→n+1**:
1. Update surface water depth h_s from water balance on runoff layer control volumes using Darcy‑film flux q_s = −K_r ∇η_s.
2. Compute ζ = min(h_s, ζ_max) and connectivity C(ζ) = 1/(1+exp[−a_c(ζ−ζ_c)]).
3. Assemble K_eff(θ,I,ζ) = K_mat(θ) · M_I^K(θ,geom) · C(ζ) with anisotropy per intervention; adjust storage via ϕ_eff = ϕ + Δϕ_I.
4. Picard loop: given θ^k, solve for head h^k = ψ(θ^k)+z with implicit diffusion, S_I sources, and continuity at surface (link to runoff layer unknowns). Update θ^{k+1} from retention curve.
5. Apply evaporation sink E_mulch = κ_I E_bare at surface nodes where I ∈ {mulch, rock mulch}.
6. Converge ||θ^{k+1}−θ^k|| < ε; advance.

**Diagnostics**: Partition infiltration‑ vs saturation‑excess; event classes (small/medium/large) using rainfall percentiles; compute annual volume capture.

---

## 7. Canonical Test Cases (Definitions, Expected Behavior)

### Test 1: Swale Annual Performance under Rainfall Percentiles
- **Initial**: K_s = 1.5 cm h⁻¹; IA = 0.6 cm; f_w = 0.4; design slope/width/depth typical.
- **Expected**: Smallest ~40% of events fully infiltrated, next ~40% partially reduced, largest ~20% little attenuation; annual volume reduction ≈ 50–70%. ([3.1], [3.2])

### Test 2: Gravel–Sand Mulch vs Bare Soil (Event Storms + 14‑Day Drying)
- **Initial**: Identical soils; apply mulch cover 100% on treated plot.
- **Expected**: Runoff reduction factor ≈ 0.05–0.15; infiltration depth ≈6×; cumulative 14‑day evaporation ≈4× lower on mulch. ([4.1], [4.2])

### Test 3: Microtopography Fill‑and‑Spill Switching
- **Initial**: Impose ζ_c = 10 mm, a_c = 1 mm⁻¹.
- **Expected**: Subsurface dominated hydrograph below ζ_c; rapid activation of surface connectivity when ζ>ζ_c; hysteretic groundwater–discharge relation; transmissivity rises as water table approaches surface. ([2.1])

### Test 4: Unified PDE Reproduces Both Hortonian and Dunne Runoff
- **Initial**: Vary rainfall intensity and antecedent θ.
- **Expected**: Infiltration‑excess when intensity > infiltration capacity (low θ); saturation‑excess when water table rises near surface (high θ), without switching equations (single PDE continuity). ([1.1])

### Test 5: Vegetation Threshold and Water‑Yield Jump (Qualitative)
- **Initial**: Couple ODEs with increasing SOM via η_1>0 and mulch/swale interventions increasing θ.
- **Expected**: Once θ and SOM exceed thresholds (θ*; SOM*), system crosses into positive feedback V–SOM–θ loop with accelerated V increase and reduced runoff; this defines a saddle‑node–like transition. ([2.1])

---

## 8. Failure Modes & Applicability Limits

- Extremely high rainfall intensities may exceed diffusive‑film assumptions for overland flow; shallow‑water equations may be required at very high Froude numbers.
- Coarse blocky soils with deep macropores may demand dual‑permeability (matrix–macropore) extensions rather than a single‑domain K_eff; transmissivity function T(η) partly mitigates via exponential activation. ([2.1])
- Sparse or absent microtopography (ζ_c→0) collapses to near‑uniform connectivity; connectivity function should degrade to C→1.
- Where interventions are absent (I = none), set multipliers to unity and κ_I = 1 to recover the standard generalized Richards behavior. ([1.1])

---

## 9. Notes on Uncertainties / Risk

- Swale performance shows large cross‑site variability (9–100%); adopt probabilistic rainfall percentile calibration and account for rill‑dominated wetted fractions rather than assuming sheet flow. ([3.1], [3.2], [5.1])
- Mulch effects are robust in semiarid loess plots (strong runoff reduction and evaporation suppression), but scale with cover fraction, particle size, and storm characteristics; interception (Δζ) depends on pebble size distribution. ([4.1], [4.2])
- Parameterization of vegetation–SOM couplings is currently placeholder; LP‑specific time‑series ingestion is needed to set r_V, a1, a2, η1 and identify V_crit, SOM* quantitatively.

---

## 10. Where This Diverges from Standard Models

- Single unified PDE with a runoff porous layer replaces ad‑hoc switching between Hortonian and Dunne mechanisms and embeds microtopographic thresholds explicitly. ([1.1], [2.1])
- Interventions directly modulate K(θ) tensor and storage via physically interpretable multipliers and surface connectivity, rather than external post‑processing of runoff.
- Empirical earthwork multipliers (mulch and swales) constrain model behavior and yield falsifiable event‑class predictions. ([3.1], [3.2], [4.1], [4.2])

---

## Partial Items Pending Additional Evidence

- Loess Plateau vegetation–SOM–θ parameter ranges and tipping thresholds (V_crit, SOM*, infiltration lock‑in): we provided equation structure and tests but require ingestion of LP time‑series and process syntheses to finalize ranges.

---

## References

[1.1] A generalized Richards equation for surface/subsurface flow modelling. S. Weill, E. Mouche, Jérémy Patin. Journal of Hydrology (2009). https://doi.org/10.1016/j.jhydrol.2008.12.007

[1.2] A generalized Richards equation for surface/subsurface flow modelling. S. Weill, E. Mouche, Jérémy Patin. Journal of Hydrology (2009). https://doi.org/10.1016/j.jhydrol.2008.12.007

[2.1] Effects of micro-topography on surface–subsurface exchange and runoff generation in a virtual riparian wetland — A modeling study. S. Frei, G. Lischeid, J.H. Fleckenstein. Advances in Water Resources (2010). https://doi.org/10.1016/j.advwatres.2010.07.006

[3.1] Enhancement and application of the Minnesota dry swale calculator. M Garcia-Serrana, JS Gulliver, JL Nieber. 2016. https://conservancy.umn.edu/bitstreams/56ec341c-e0f5-4e4a-8f65-fde41a61fd27/download

[3.2] Enhancement and application of the Minnesota dry swale calculator. M Garcia-Serrana, JS Gulliver, JL Nieber. 2016. https://conservancy.umn.edu/bitstreams/56ec341c-e0f5-4e4a-8f65-fde41a61fd27/download

[4.1] Gravel–sand mulch for soil and water conservation in the semiarid loess region of northwest China. Xiao-Yan Li. Catena (2003). https://doi.org/10.1016/s0341-8162(02)00181-9

[4.2] Gravel–sand mulch for soil and water conservation in the semiarid loess region of northwest China. Xiao-Yan Li. Catena (2003). https://doi.org/10.1016/s0341-8162(02)00181-9

[5.1] Analysis of Infiltration and Overland Flow over Sloped Surfaces: Application to Roadside Swales. MDC Garcia de la Serrana Lozano. 2017. https://conservancy.umn.edu/bitstreams/ef28f19c-5c96-43d8-b367-8c2e3057ab31/download

---

**This document provides the scientific foundation for the HYD-RLv1 (Richards-Lite) hydrology solver implementation in negentropic-core.**
