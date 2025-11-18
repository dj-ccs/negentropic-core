# PHYS-v2.2 Progress Tracker
# Track 1: The Soul — Structure-Preserving Physics Core

**Sprint**: v2.2 "Soul and Skin"
**Track**: 1 (Physics)
**Duration**: Weeks 1-3
**Status**: 🔴 Not Started

---

## Overview

This document tracks the implementation progress of the structure-preserving physics core for v2.2. All tasks are defined in `docs/v2.2_Upgrade.md` Track 1.

**Key Deliverables**:
- Torsion kernel (discrete curl + tendencies)
- RKMK4 integrator for SE(3)
- Clebsch-Collective integrator (symplectic + LUT)
- LoD-gated dispatch
- Science ports (Medlyn, F:B, dual-porosity)
- Full conservation test suite

---

## Week 1: Kernels & Foundations

### Phase 1.1: Torsion Kernel ⬜ Not Started

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 1.1.1: Create `torsion.h` API | ⬜ | - | Exact spec from integrators.md |
| 1.1.2: Implement discrete curl | ⬜ | - | CliMA weak curl, 5-7 pt stencil |
| 1.1.3: Implement torsion tendency | ⬜ | - | Conservative momentum (α=1e-3) |
| 1.1.4: Update SAB writer | ⬜ | - | Write torsion to SAB |
| 1.1.5: Create unit tests | ⬜ | - | Analytic checks, L2 error < 1e-4 |

**Validation Checklist**:
- [ ] All unit tests pass (3/3)
- [ ] Performance: <50 cycles/cell
- [ ] SAB torsion field populated
- [ ] No memory leaks (valgrind)

**Blockers**: None

---

### Phase 1.2: RKMK4 for SE(3) ⬜ Not Started

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 1.2.1: Create `integrators.h` API | ⬜ | - | Master integrator header |
| 1.2.2: Implement RKMK4 core | ⬜ | - | 4th-order, BCH truncation |
| 1.2.3: Create exp_map LUT | ⬜ | - | 8192 entries, 32 KB |
| 1.2.4: Implement workspace mgmt | ⬜ | - | Scratch space allocation |
| 1.2.5: Create integration tests | ⬜ | - | Orthogonality, energy |

**Validation Checklist**:
- [ ] Orthogonality error < 1e-10 (10k steps)
- [ ] Energy drift < 1e-6/year
- [ ] Performance: <100 ns/step
- [ ] Deterministic across platforms

**Blockers**: None

---

## Week 2: Geometric Core & Dispatch

### Phase 1.3: Clebsch-Collective Integrator ⬜ Not Started

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 1.3.1: Implement Clebsch lift | ⬜ | - | LP → canonical, table-driven |
| 1.3.2: Implement Clebsch project | ⬜ | - | canonical → LP, Casimir correction |
| 1.3.3: Implement symplectic RK | ⬜ | - | Velocity-Verlet, bounded Newton |
| 1.3.4: Generate Clebsch LUT | ⬜ | - | SymPy solver, 256-512 bins |
| 1.3.5: Create conservation tests | ⬜ | - | Casimir, energy, reversibility |

**Validation Checklist**:
- [ ] Casimir drift < 1e-6 (fixed-point)
- [ ] Performance: <220 ns/cell (WASM)
- [ ] LUT memory: <8 KB
- [ ] All conservation tests pass (3/3)

**Blockers**: None

**Open Decisions**:
- [ ] LUT size: 256 or 512 bins? (Recommendation: 256)
- [ ] Fixed-point or FP64 for Clebsch internals? (Recommendation: FP64)

---

### Phase 1.4: LoD Gating & Dispatch ⬜ Not Started

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 1.4.1: Modify state_step dispatcher | ⬜ | - | LoD-based integrator selection |
| 1.4.2: Implement error escalation | ⬜ | - | Dynamic upgrade RK4→RKMK4→Clebsch |
| 1.4.3: Create LoD integration test | ⬜ | - | Verify correct dispatch |

**Validation Checklist**:
- [ ] Correct integrator per LoD level
- [ ] Torsion only computed for LoD ≥ 2
- [ ] Escalation reduces error
- [ ] Performance: <10% overhead vs v0.3.3

**Blockers**: Depends on Phase 1.1-1.3 completion

**Open Decisions**:
- [ ] Should momentum coupling α be LoD-dependent?

---

## Week 3: Science & Validation

### Phase 1.5: Science Ports ⬜ Not Started

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 1.5.1: Port Medlyn model | ⬜ | - | Stomatal conductance |
| 1.5.2: Create PFT parameter table | ⬜ | - | g1 values for different biomes |
| 1.5.3: Integrate into transpiration | ⬜ | - | REGv2 update |
| 1.5.4: Implement F:B response | ⬜ | - | Continuous Monod kinetics |
| 1.5.5: Replace F:B LUT | ⬜ | - | Remove 8-entry table |
| 1.5.6: Validate against CliMA | ⬜ | - | Benchmark comparison |
| 1.5.7: Implement dual-porosity K | ⬜ | - | Aggregation enhancement |
| 1.5.8: Add aggregation calculation | ⬜ | - | SOM-driven factor |
| 1.5.9: Validate infiltration | ⬜ | - | High-SOM cell check |

**Validation Checklist**:
- [ ] Medlyn gs integrated
- [ ] F:B ratio drives SOM continuously
- [ ] Dual-porosity K improves infiltration
- [ ] All solver tests pass (regression)

**Blockers**: None (can proceed in parallel with 1.1-1.4)

---

### Phase 1.6: CI/CD & Validation ⬜ Not Started

| Task | Status | Owner | Notes |
|------|--------|-------|-------|
| 1.6.1: Implement all test files | ⬜ | - | 7 new test files |
| 1.6.2: Update CI/CD pipeline | ⬜ | - | GitHub Actions workflow |
| 1.6.3: Create perf regression detector | ⬜ | - | Fail if >220 ns/cell |
| 1.6.4: Document validation protocol | ⬜ | - | integrators_math_appendix.md |

**Validation Checklist**:
- [ ] All conservation tests pass
- [ ] All performance benchmarks meet targets
- [ ] CI pipeline runs successfully
- [ ] No regressions in existing tests

**Test Coverage**:
```
tests/
├── integrator_conservation_test.c   ⬜
├── integrator_reversibility_test.c  ⬜
├── torsion_unit_test.c              ⬜
├── test_baroclinic_wave.c           ⬜
├── test_rkmk4.c                     ⬜
├── test_lod_dispatch.c              ⬜
└── test_clebsch_collective.c        ⬜
```

**Blockers**: Depends on all other phases

---

## Overall Progress

### Completion Metrics

| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| Torsion kernel | Complete | 0% | 🔴 |
| RKMK4 integrator | Complete | 0% | 🔴 |
| Clebsch-Collective | Complete | 0% | 🔴 |
| LoD dispatch | Complete | 0% | 🔴 |
| Science ports | Complete | 0% | 🔴 |
| Tests passing | 100% | 0% | 🔴 |
| Conservation tests | Pass | - | - |
| Performance benchmarks | Pass | - | - |

---

## Performance Targets (Must Meet)

| Component | Target | Current | Status |
|-----------|--------|---------|--------|
| Torsion kernel | <50 cycles/cell | - | - |
| RKMK4 | <100 ns/step | - | - |
| Clebsch-Collective | <220 ns/cell | - | - |
| LoD overhead | <10% vs v0.3.3 | - | - |
| Casimir drift | <1e-6 (fixed-pt) | - | - |
| Energy drift | <1e-6/year | - | - |

---

## Risks & Blockers

### Active Risks
1. **Clebsch LUT convergence** (Mitigation: Analytic fallback)
2. **Performance targets** (Mitigation: Profile early, SIMD)
3. **SAB layout extension** (Mitigation: Use reserved offset)

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
| Clebsch LUT size | 256 or 512 bins | 256 (4 KB) | Week 2 start |
| Clebsch precision | Fixed-pt or FP64 | FP64 internal | Week 2 start |
| Momentum coupling α | Constant or LoD-scaled | Start constant | Week 1 end |

---

## Weekly Standup Notes

### Week 1 (2025-11-18)
- **Status**: Sprint kick-off
- **This week**: Phase 1.1 (Torsion) + Phase 1.2 (RKMK4)
- **Blockers**: None
- **Notes**: -

### Week 2 (2025-11-25)
- **Status**: -
- **This week**: -
- **Blockers**: -
- **Notes**: -

### Week 3 (2025-12-02)
- **Status**: -
- **This week**: -
- **Blockers**: -
- **Notes**: -

---

## Commit Log

| Date | Commit | Description |
|------|--------|-------------|
| 2025-11-18 | - | Planning phase, no commits yet |

---

## Contact

- **Track Lead**: Physics Team Lead
- **Contributors**: -
- **Code Reviews**: Technical Lead
- **Questions**: GitHub Discussions

---

**Last Updated**: 2025-11-18
**Next Update**: End of Week 1
**Status Legend**: ⬜ Not Started | 🟡 In Progress | ✅ Complete | 🔴 Blocked
