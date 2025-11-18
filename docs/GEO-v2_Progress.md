# GEO-v2 Progress Tracker
# Track 2: The Skin — Living Globe Interface

**Sprint**: v2.2 "Soul and Skin"
**Track**: 2 (Visualization & UI)
**Duration**: Weeks 2-4
**Status**: 🔴 Not Started

---

## Overview

This document tracks the implementation progress of the interactive user-facing features for v2.2. All tasks are defined in `docs/v2.2_Upgrade.md` Track 2.

**Key Deliverables**:
- Difference Map layer (restoration visualization)
- Scenario Editor control panel (parameter tuning)
- Intervention Event system (hash-chained log + replay)
- Visual Torsion twist (GPU-based turbulence)

---

## Week 2: Core Visualization Features

### Phase 2.1: Difference Map Layer ⬜ Not Started
**Estimated Time**: 4 hours

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 2.1.1: Create DifferenceMapLayer class | ⬜ | - | Custom deck.gl layer |
| 2.1.2: Implement custom shaders | ⬜ | - | Difference calc + color mapping |
| 2.1.3: Add UI toggle | ⬜ | - | "Show Restoration" button |
| 2.1.4: Integrate with render worker | ⬜ | - | Pass baseline/current textures |

**Validation Checklist**:
- [ ] Difference map renders correctly
- [ ] Green = restoration, red = degradation
- [ ] Toggle works without crashes
- [ ] Performance: 60 FPS @ 100×100 grid

**Implementation Files**:
```
web/src/layers/
├── DifferenceMapLayer.ts         ⬜
└── shaders/
    ├── difference-vertex.glsl    ⬜
    └── difference-fragment.glsl  ⬜
```

**Blockers**: None

---

### Phase 2.2: Scenario Editor Control Panel ⬜ Not Started
**Estimated Time**: 4 hours

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 2.2.1: Create ScenarioEditor class | ⬜ | - | dat.GUI wrapper |
| 2.2.2: Implement param change handler | ⬜ | - | Core worker messages |
| 2.2.3: Create preset scenarios | ⬜ | - | 3 presets minimum |
| 2.2.4: Add localStorage persistence | ⬜ | - | Save/load settings |

**Validation Checklist**:
- [ ] All parameter groups display
- [ ] Sliders update simulation real-time
- [ ] Presets load correctly
- [ ] Settings persist across sessions

**Implementation Files**:
```
web/src/ui/
├── ScenarioEditor.ts      ⬜
├── ParameterGroups.ts     ⬜
└── ScenarioPresets.ts     ⬜
```

**Parameter Groups**:
- [ ] Vegetation (r_V, K_V, lambda1)
- [ ] Soil (eta1, a1, a2)
- [ ] Hydrology (K_sat, theta_sat)

**Preset Scenarios**:
- [ ] "Baseline Degraded"
- [ ] "Aggressive Restoration"
- [ ] "Loess Plateau Historical"

**Blockers**: None

---

## Week 3: Event System & Visual Effects

### Phase 2.3: Intervention Event System ⬜ Not Started
**Estimated Time**: 6 hours

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 2.3.1: Create Event Logger class | ⬜ | - | Hash-chained events |
| 2.3.2: Implement hash chaining | ⬜ | - | SHA-256 via SubtleCrypto |
| 2.3.3: Implement "Share Scenario" | ⬜ | - | LZMA + base64 + URL |
| 2.3.4: Implement replay system | ⬜ | - | Validate + apply events |
| 2.3.5: Add event hooks | ⬜ | - | Param change, camera, etc. |

**Validation Checklist**:
- [ ] Event log captures all actions
- [ ] Hash chain validates correctly
- [ ] Shared URLs load and replay
- [ ] Event overhead < 1%
- [ ] Replay produces identical state hash

**Implementation Files**:
```
web/src/events/
├── EventLogger.ts      ⬜
└── EventReplayer.ts    ⬜

web/src/ui/
└── ShareButton.tsx     ⬜
```

**Event Types Implemented**:
- [ ] session_start / session_end
- [ ] place_intervention
- [ ] remove_intervention
- [ ] change_parameter
- [ ] camera_move (debounced)
- [ ] simulation_step
- [ ] checkpoint

**Blockers**: None

---

### Phase 2.4: Visual Torsion Twist ⬜ Not Started
**Estimated Time**: 2 hours

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 2.4.1: Modify cloud particle shader | ⬜ | - | Add torsion texture |
| 2.4.2: Pass torsion data to shader | ⬜ | - | WebGL texture from SAB |
| 2.4.3: Add turbulence intensity control | ⬜ | - | UI slider (0.0-2.0) |

**Validation Checklist**:
- [ ] Clouds show turbulence in high-torsion regions
- [ ] Effect scales with torsion magnitude
- [ ] No performance impact (60 FPS @ 100k particles)
- [ ] Intensity control works

**Implementation Files**:
```
web/src/layers/shaders/
└── cloud-particles-fragment.glsl  ⬜ (modified)
```

**Blockers**: Depends on Track 1 Phase 1.1 (torsion SAB field)

---

## Week 4: Integration & Polish

### Integration Tasks ⬜ Not Started

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| UI/UX polish | ⬜ | - | Consistent styling |
| Error handling | ⬜ | - | Graceful degradation |
| Performance profiling | ⬜ | - | 60 FPS guarantee |
| Cross-browser testing | ⬜ | - | Chrome, Firefox, Safari |
| Mobile responsiveness | ⬜ | - | Touch controls |

**Validation Checklist**:
- [ ] All features work together
- [ ] No visual glitches
- [ ] Performance maintained
- [ ] Works on all target browsers

**Blockers**: None

---

## Overall Progress

### Completion Metrics

| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| Difference Map | Complete | 0% | 🔴 |
| Scenario Editor | Complete | 0% | 🔴 |
| Event System | Complete | 0% | 🔴 |
| Visual Torsion | Complete | 0% | 🔴 |
| Integration | Complete | 0% | 🔴 |

---

## Performance Targets (Must Meet)

| Component | Target | Current | Status |
|-----------|--------|---------|--------|
| Render FPS | 60 FPS | - | - |
| Difference Map | 60 FPS @ 100×100 | - | - |
| Cloud particles | 60 FPS @ 100k | - | - |
| Event log overhead | <1% | - | - |
| UI responsiveness | <100ms | - | - |

---

## Feature Demonstrations

### Difference Map Demo
**Scenario**: Loess Plateau 1995-2010
- [ ] Load baseline (1995 degraded state)
- [ ] Run 10-year restoration
- [ ] Toggle "Show Restoration"
- [ ] Verify green regions show vegetation increase
- [ ] Verify red regions show degradation (if any)

### Scenario Editor Demo
**Scenario**: Parameter sweep
- [ ] Open Scenario Editor
- [ ] Adjust r_V slider (0.15 → 0.50)
- [ ] Observe vegetation growth acceleration
- [ ] Load "Aggressive Restoration" preset
- [ ] Verify all parameters update

### Event System Demo
**Scenario**: Share and replay
- [ ] Perform 5-10 actions (param changes, interventions)
- [ ] Click "Share Scenario"
- [ ] Copy URL
- [ ] Open in new tab / browser
- [ ] Verify identical state is reproduced

### Visual Torsion Demo
**Scenario**: High-wind region
- [ ] Navigate to region with strong wind field
- [ ] Observe cloud turbulence
- [ ] Adjust torsion intensity slider
- [ ] Verify visual effect scales correctly

---

## Dependencies

### From Track 1 (PHYS-v2.2)
- [ ] **Torsion SAB field** (for Phase 2.4)
  - Required by: Visual Torsion shader
  - Status: ⬜ Not started (Track 1 Phase 1.1)
  - Fallback: Use stub zero-torsion data

### External Libraries
- [ ] dat.GUI (add to package.json)
- [ ] LZMA compression (lzma-native or similar)
- [ ] Existing: deck.gl, gl-matrix, CesiumJS

---

## Risks & Blockers

### Active Risks
1. **LZMA compression overhead** (Mitigation: Test early, use simpler compression if needed)
2. **URL length limits** (Mitigation: Warn user if >2048 chars, suggest alternative)
3. **Shader complexity** (Mitigation: Start simple, add features incrementally)

### Resolved Risks
None yet.

### Current Blockers
None.

---

## Key Decisions Made

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| - | - | - | - |

---

## Key Decisions Pending

| Decision | Options | Recommendation | Target Date |
|----------|---------|----------------|-------------|
| Event compression | LZMA vs simpler | LZMA for "Share", JSON for local | Week 3 start |
| Torsion intensity default | 0.5, 1.0, or 2.0 | 1.0 (medium effect) | Week 3 end |
| Mobile support scope | Full or desktop-only | Desktop priority, mobile nice-to-have | Week 4 start |

---

## Weekly Standup Notes

### Week 1 (2025-11-18)
- **Status**: Planning phase (Track 2 starts Week 2)
- **This week**: Preparation, dependency checks
- **Blockers**: None
- **Notes**: Waiting for Track 1 to establish torsion SAB field

### Week 2 (2025-11-25)
- **Status**: -
- **This week**: Phase 2.1 (Difference Map) + Phase 2.2 (Scenario Editor)
- **Blockers**: -
- **Notes**: -

### Week 3 (2025-12-02)
- **Status**: -
- **This week**: Phase 2.3 (Event System) + Phase 2.4 (Visual Torsion)
- **Blockers**: -
- **Notes**: -

### Week 4 (2025-12-09)
- **Status**: -
- **This week**: Integration + Polish
- **Blockers**: -
- **Notes**: -

---

## Commit Log

| Date | Commit | Description |
|------|--------|-------------|
| 2025-11-18 | - | Planning phase, no commits yet |

---

## Contact

- **Track Lead**: Visualization Team Lead
- **Contributors**: -
- **Code Reviews**: Technical Lead
- **Questions**: GitHub Discussions

---

**Last Updated**: 2025-11-18
**Next Update**: End of Week 2
**Status Legend**: ⬜ Not Started | 🟡 In Progress | ✅ Complete | 🔴 Blocked
