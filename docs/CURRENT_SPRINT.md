# Sprint: GEO-v1 Integration (Week of Nov 17)

**Goal:** Get simulation running in browser with basic visualization.

**Success Criteria:**
- [x] **Simulation compiled to WASM** ✅ (stub integration, physics pending)
- [ ] Simulation running in Core Worker
- [ ] Data flowing from simulation to SharedArrayBuffer
- [ ] deck.gl rendering colored grid based on simulation state
- [ ] Play/pause/speed controls functional

---

## Tasks

### WASM Compilation (Est: 1 day) ✅ COMPLETE
- [x] Install Emscripten SDK (v4.0.19)
- [x] Updated CMakeLists.txt to include HYD-RLv1 and REGv1 solvers
- [x] Compile `negentropic-core` to WASM module (using existing CMake config)
- [x] Test: Load WASM in browser, call exported functions
- [x] Benchmark: Initial performance baseline established

**Outputs:**
- `build/wasm/negentropic_core.wasm` (44KB)
- `build/wasm/negentropic_core.js` (13KB loader)
- Test harness: `build/wasm/test.html` and `test.js`

**Findings:**
- WASM compilation successful with all solvers (HYD-RLv1, REGv1, ATMv1)
- Basic API functional: `neg_create`, `neg_step`, `neg_destroy`, `neg_get_state_hash`
- Current `state_step()` is a stub (no physics integration yet)
- Performance baseline: ~839ns/step overhead (1.19M steps/sec)
- JSON parsing is basic - only handles minimal configs currently

**Next:** Implement actual physics integration in `state_step()` to call HYD-RLv1/REGv1 solvers

### SharedArrayBuffer Setup (Est: 1 day)
- [ ] Allocate SAB in main thread (size from Memory spec)
- [ ] Transfer SAB to Core Worker
- [ ] Implement write: simulation state → SAB
- [ ] Implement read: SAB → Render Worker
- [ ] Test: Verify atomic operations work correctly

### Visualization (Est: 1-2 days)
- [ ] Create `SimulationGridLayer.ts` extending deck.gl
- [ ] Implement getters: read theta/vegetation/som from SAB
- [ ] Color mapping: theta → blue gradient, vegetation → green
- [ ] Add layer to deck instance
- [ ] Test: Colors update as simulation runs

### Demo Loop (Est: 1 day)
- [ ] Load Loess Plateau initial conditions
- [ ] Start simulation on button click
- [ ] Add time controls (play/pause/1x/10x/100x)
- [ ] Display current sim year
- [ ] Record demo video

---

## Blockers
None currently.

## Notes
- Using hardcoded Loess Plateau initial state (skip Prithvi for now)
- Simple color mapping first (defer fancy terrain shader)
- Focus: prove end-to-end integration works
