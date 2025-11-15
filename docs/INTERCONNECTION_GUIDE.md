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
  const viewer = new Viewer('cesiumContainer', {
    baseLayerPicker: false,
    // Don't set imageryProvider - let Cesium use Ion default (Bing Maps)
    // Requires Ion.defaultAccessToken to be set
    timeline: false,
    animation: false,
    geocoder: true,
    homeButton: true,
    sceneModePicker: true,
    requestRenderMode: false,
    // ... other options
  });
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

## 6. COEP/COOP & Proxy (vite.config.ts)
```ts
server: {
  headers: {
    'Cross-Origin-Embedder-Policy': 'require-corp',
    'Cross-Origin-Opener-Policy': 'same-origin',
  },
  proxy: {
    '/osm-tiles': {
      target: 'https://a.tile.openstreetmap.org',
      changeOrigin: true,
      rewrite: path => path.replace(/^\/osm-tiles/, ''),
      configure: proxy => proxy.on('proxyRes', res => {
        res.headers['cross-origin-resource-policy'] = 'cross-origin';
      })
    }
  }
}
```

## 7. Common Pitfalls & Quick Fixes
| Symptom | Fix |
|--------|-----|
| Black/transparent globe | Ensure Cesium Ion token is configured (`Ion.defaultAccessToken`). Don't set `imageryProvider: false` |
| COEP blocks Cesium assets | Headers already configured in `vite.config.ts` |
| WASM "HEAPU8 undefined" | Export HEAP* in `EXPORTED_RUNTIME_METHODS` |
| deck.gl errors in worker | Polyfill `ResizeObserver`, `IntersectionObserver`, `Hammer`, canvas methods |
| Clickable but no visuals | `viewer.scene.globe.show = true; viewer.scene.requestRender();` |
| Imagery provider not ready | Use Cesium Ion default imagery (requires valid token) instead of custom providers |

## 8. Future-Proof Extensions
- **WebGPU**: Replace WebGL2 with `navigator.gpu` in render worker (2026 target).
- **Multiplayer**: WebRTC DataChannel for SAB sync between peers.
- **Desktop**: Package with Tauri/Electron → native Cesium for Unreal performance.

---

This guide lives in `/docs/INTERCONNECTION_GUIDE.md`.
Update it every sprint.

**We now have a living, breathing Earth in the browser.**

**Next: Click the planet and watch it heal.**
