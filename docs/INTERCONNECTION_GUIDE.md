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
  import { createWorldImagery } from 'cesium';

  const imageryProvider = await createWorldImagery();

  const viewer = new Viewer('cesiumContainer', {
    baseLayerPicker: false,
    imageryProvider: imageryProvider,  // MUST pass explicitly to guarantee layer 0
    timeline: false,
    animation: false,
    geocoder: true,
    homeButton: true,
    sceneModePicker: true,
    requestRenderMode: false,
    // ... other options
  });

  // Post-creation hardening
  viewer.scene.globe.show = true;
  viewer.scene.render();
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
| Symptom | Fix |
|--------|-----|
| **Invisible globe (stars visible, no Earth)** | **CRITICAL FIX**: Use `createWorldImagery()` and pass to Viewer constructor. See code in Section 3. |
| Cannot read properties of undefined (reading 'ready') | Add null check before accessing `imageryProvider.ready` - provider may be undefined during initialization |
| Black/transparent globe | Ensure Cesium Ion token is configured (`Ion.defaultAccessToken`). Verify `imageryLayers.length > 0` |
| COEP blocks Cesium assets | Headers already configured in `vite.config.ts` |
| WASM "HEAPU8 undefined" | Export HEAP* in `EXPORTED_RUNTIME_METHODS` |
| deck.gl errors in worker | Polyfill `ResizeObserver`, `IntersectionObserver`, `Hammer`, canvas methods |
| Clickable but no visuals | `viewer.scene.globe.show = true; viewer.scene.requestRender();` |
| OSM/UrlTemplateImageryProvider not ready | Don't use OSM with COEP/COOP - use Ion imagery instead (more reliable) |

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
