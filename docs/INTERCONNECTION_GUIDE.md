# Negentropic-Core GEO-v1 Interconnection Guide (Nov 2025)

## 1. High-Level Architecture (3-Thread Zero-Copy Pipeline)
```
Main Thread (CesiumJS + UI)
   │
   ├─► Core Worker (negentropic_core.wasm)          ← physics & regeneration
   │       SharedArrayBuffer (SAB) + Atomics.notify()
   │
   └─► Render Worker (deck.gl on OffscreenCanvas)    ← analytical overlays
           SharedArrayBuffer (read-only) + Atomics.waitAsync()
```
- **Why this pattern?** 60 FPS globe + 10 Hz simulation + continental scale without main-thread jank.
- **COEP/COOP required** for SAB → Vite headers + proxy for third-party tiles.

## 2. WASM Core (negentropic_core.wasm)
- **Build**: `./web/build-wasm.sh` (Emscripten 4.0.19+)
  - `-s MODULARIZE=1 -s EXPORT_ES6=1 -s PTHREAD_POOL_SIZE=2`
  - Exports: `createNegentropic(Module)` factory
- **Memory Layout (SAB)**:
  - 128-byte header (timestamp u64, version u32, field offsets)
  - Typed arrays: Float32Array for theta, SOM, vegetation, temperature
- **Initialization in Core Worker** (core-worker.ts):
  ```ts
  const Module = { onRuntimeInitialized: () => postMessage({type: 'WASM_READY'}) };
  const instance = createNegentropic(Module);  // <-- factory call
  ```
- **Step Loop**:
  ```c
  neg_step(simHandle, dt);
  Atomics.store(header, 0, Date.now());
  Atomics.notify(header, 0);
  ```

## 3. CesiumJS Globe (Main Thread)
- **Viewer Creation** (main.ts):
  ```ts
  // CRITICAL: Explicitly create Ion imagery provider before Viewer
  // This is THE FIX for invisible globes - see Common Pitfalls section
  import { IonImageryProvider } from 'cesium';

  // Asset ID 2 = Bing Maps Aerial with Labels (Ion default)
  const imageryProvider = await IonImageryProvider.fromAssetId(2);

  const viewer = new Viewer('cesiumContainer', {
    baseLayerPicker: false,
    imageryProvider: imageryProvider,  // Pass to constructor (primary method)
    baseLayer: true,                   // Enable base rendering (CRITICAL for visibility)
    timeline: false,
    animation: false,
    geocoder: true,
    homeButton: true,
    sceneModePicker: true,
    requestRenderMode: false,          // Continuous 60 FPS
    // ... other options
  });

  // FALLBACK: Explicitly add imagery layer if constructor didn't add it
  // In CesiumJS v1.120+, passing to constructor may not always add layer 0
  if (viewer.imageryLayers.length === 0) {
    console.warn('⚠ Imagery not added via constructor, adding explicitly...');
    viewer.imageryLayers.addImageryProvider(imageryProvider);
  }

  // Post-creation hardening
  viewer.scene.globe.show = true;
  viewer.scene.requestRender();  // Force first render
  ```
- **Click → ROI**:
  ```ts
  viewer.screenSpaceEventHandler.setInputAction((click) => {
    const cartesian = viewer.camera.pickEllipsoid(click.position);
    const bbox = compute10kmBbox(cartesian);
    coreWorker.postMessage({ type: 'INIT_SIM', bbox, config });
  }, ScreenSpaceEventType.LEFT_CLICK);
  ```

## 4. deck.gl Overlays (Render Worker + OffscreenCanvas)
- **Transfer** (main.ts — ONE TIME):
  ```ts
  const offscreen = canvas.transferControlToOffscreen();
  renderWorker.postMessage({ type: 'INIT_CANVAS', canvas: offscreen }, [offscreen]);
  ```
- **Polyfills** (render-worker.ts — top):
  ```ts
  // IntersectionObserver, ResizeObserver, Hammer.js no-ops
  // OffscreenCanvas extensions: getBoundingClientRect, style.touchAction, offsetWidth/Height
  ```
- **Deck Init**:
  ```ts
  const deck = new Deck({
    canvas: offscreenCanvas,
    controller: false,
    layers: [
      new GridLayer({ data: new Float32Array(sab, offset_theta, cells), getFillColor: d => moistureColor(d) })
    ]
  });
  ```

## 5. Prithvi Adapter (Initialization & Validation)
- **Load** (main or worker):
  ```ts
  import { AutoModelForImageSegmentation } from '@xenova/transformers';
  const model = await AutoModelForImageSegmentation.from_pretrained('ibm-nasa-geospatial/prithvi-100M-flood');
  const { pixel_values } = await preprocessor(hlsTile);  // 6-band HLS
  const output = await model(pixel_values);
  // Map masks → initial vegetation/SOM
  ```
- **Validation**: IoU between simulated water and Prithvi flood mask.

## 6. COEP/COOP Headers (vite.config.ts)
```ts
server: {
  headers: {
    'Cross-Origin-Embedder-Policy': 'require-corp',
    'Cross-Origin-Opener-Policy': 'same-origin',
  }
}
```
**Note**: No proxy needed for Cesium Ion imagery - tiles are served with proper CORS headers.
OSM tiles require proxy but Ion is more reliable in production.

## 7. Common Pitfalls & Quick Fixes
| Symptom | Logs/Console | Fix |
|---------|--------------|-----|
| **Invisible Globe (Stars Visible)** | `imageryLayers.length = 0`, `ready: false` | Pass `imageryProvider` to constructor AND set `baseLayer: true`; add via `viewer.imageryLayers.addImageryProvider(provider)` as fallback; hard refresh |
| **COEP Blocks Tiles/Assets** | "Specify a Cross-Origin Embedder Policy" warnings | Vite proxy + CORP header; serve Cesium locally (`npm i cesium`) |
| **WASM HEAPU8 Undefined** | "Cannot read properties of undefined (reading 'set')" | Export `HEAPU8, HEAP8, etc.` in `-sEXPORTED_RUNTIME_METHODS`; call `createNegentropic(Module)` after eval(glue) |
| **deck.gl Worker Errors** | "ResizeObserver undefined", "getBoundingClientRect not a function" | Polyfill ResizeObserver/IntersectionObserver; add `offscreen.getBoundingClientRect = () => ({width: 1024, height: 1024})` |
| **OffscreenCanvas Transfer Fail** | "Cannot transfer control more than once" | Transfer **once** with flag; create new canvas on error/reconnect |
| **Slow/Blank Mobile** | High zoom, no tiles | Set `maximumLevel: 14`; enable WebGL2 in browser flags; use `requestRenderMode: true` for throttled FPS |
| **Click Works, No Sim** | "WASM_READY" but no plume | Check SAB notify: `Atomics.notify(sabView, 0)`; verify offset in Float32Array |
| Cannot read properties of undefined (reading 'ready') | Provider undefined during init | Add null check: `if (provider) { ... }` before accessing `provider.ready` |

## 8. Future-Proof Extensions
- **WebGPU**: Replace WebGL2 with `navigator.gpu` in render worker (2026 target).
- **Multiplayer**: WebRTC DataChannel for SAB sync between peers.
- **Desktop**: Package with Tauri/Electron → native Cesium for Unreal performance.

---

This guide lives in `/docs/INTERCONNECTION_GUIDE.md`.
Update it every sprint.

**Last Updated**: 2025-11-15 (Switched to explicit Ion imagery creation)

**Recent Changes (Nov 2025):**
- Switched from OSM to Cesium Ion imagery (more reliable with COEP/COOP)
- Added `createWorldImagery()` explicit provider creation (fixes invisible globe)
- Removed OSM proxy configuration (no longer needed)
- Added null checks for imagery provider (prevents initialization crashes)

**We now have a living, breathing Earth in the browser.**

**Next: Click the planet and watch it heal.**
