# Sprint: GEO-v1 Integration (Week of Nov 17)

**Goal:** Get simulation running in browser with basic visualization.

**Success Criteria:**
- [ ] Simulation compiled to WASM and running in Core Worker
- [ ] Data flowing from simulation to SharedArrayBuffer
- [ ] deck.gl rendering colored grid based on simulation state
- [ ] Play/pause/speed controls functional

---

## Tasks

### WASM Compilation (Est: 1 day)
- [ ] Install Emscripten SDK
- [ ] Create `build-wasm.sh` script
- [ ] Compile `negentropic-core` to WASM module
- [ ] Test: Load WASM in browser, call exported functions
- [ ] Benchmark: Compare WASM vs native performance

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
