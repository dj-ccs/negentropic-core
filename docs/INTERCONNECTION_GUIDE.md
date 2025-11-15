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

**CRITICAL: COEP-Compatible Setup Required**

Cesium requires special configuration to work with COEP (Cross-Origin-Embedder-Policy), which is mandatory for SharedArrayBuffer. See Section 6 for full details.

- **Viewer Creation** (main.ts):
  ```ts
  // STEP 1: Configure Cesium paths BEFORE creating viewer
  // This must be at the top of main.ts, before any Cesium usage
  (window as any).CESIUM_BASE_URL = '/cesium/';
  Ion.defaultServer = '/cesium-ion-api/';
  Ion.defaultAccessToken = import.meta.env.VITE_CESIUM_ION_TOKEN;

  // STEP 2: Create imagery provider explicitly
  // Asset ID 2 = Bing Maps Aerial with Labels (Ion default)
  const imageryProvider = await IonImageryProvider.fromAssetId(2);

  // STEP 3: Create viewer with provider
  const viewer = new Viewer('cesiumContainer', {
    imageryProvider: imageryProvider,  // Pass provider object to constructor
    baseLayerPicker: false,            // Disable UI picker
    timeline: false,
    animation: false,
    geocoder: true,
    homeButton: true,
    sceneModePicker: true,
    requestRenderMode: false,          // Continuous 60 FPS
    // ... other options
  });

  // STEP 4: Verify and fallback
  // In CesiumJS v1.120+, verify constructor added the layer
  if (viewer.imageryLayers.length === 0) {
    console.warn('⚠ Imagery not added via constructor, adding explicitly...');
    viewer.imageryLayers.addImageryProvider(imageryProvider);

    if (viewer.imageryLayers.length === 0) {
      throw new Error('❌ CRITICAL: Failed to add imagery layer');
    }
  }

  // STEP 5: Post-creation hardening
  viewer.scene.globe.show = true;
  viewer.scene.requestRender();  // Force first render

  console.log(`✓ Cesium globe initialized with ${viewer.imageryLayers.length} layer(s)`);
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

## 6. COEP/COOP Configuration (vite.config.ts)

**CRITICAL: Complete COEP-Compatible Setup**

COEP (Cross-Origin-Embedder-Policy) is required for SharedArrayBuffer, but it blocks Cesium's worker scripts and Ion API calls. The solution requires three components:

### 6.1 Install Dependencies
```bash
npm install --save-dev vite-plugin-static-copy
```

### 6.2 Vite Configuration
```ts
import { defineConfig } from 'vite';
import { viteStaticCopy } from 'vite-plugin-static-copy';
import wasm from 'vite-plugin-wasm';

export default defineConfig({
  plugins: [
    wasm(),
    // Copy Cesium's static assets to serve from same origin
    viteStaticCopy({
      targets: [
        { src: 'node_modules/cesium/Build/Cesium/Workers', dest: 'cesium' },
        { src: 'node_modules/cesium/Build/Cesium/ThirdParty', dest: 'cesium' },
        { src: 'node_modules/cesium/Build/Cesium/Assets', dest: 'cesium' },
        { src: 'node_modules/cesium/Build/Cesium/Widgets', dest: 'cesium' }
      ]
    }),
  ],
  server: {
    headers: {
      'Cross-Origin-Embedder-Policy': 'require-corp',
      'Cross-Origin-Opener-Policy': 'same-origin',
    },
    // CRITICAL: Proxy Cesium Ion API to make it same-origin
    proxy: {
      '/cesium-ion-api': {
        target: 'https://api.cesium.com/',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/cesium-ion-api/, ''),
      },
    },
  },
});
```

### 6.3 Why This Is Required

**Problem 1: COEP blocks Cesium worker scripts**
- Cesium's internal workers (e.g., `createVerticesFromHeightmap.js`) appear as cross-origin
- Browser blocks them with COEP violation errors
- **Solution**: Copy assets locally and serve from `/cesium/`

**Problem 2: COEP blocks Cesium Ion API calls**
- API calls to `api.cesium.com` are definitely cross-origin
- IonImageryProvider times out waiting for metadata (5+ seconds)
- **Solution**: Proxy API calls through `/cesium-ion-api/`

**Problem 3: Token security**
- API tokens should never be in source code
- **Solution**: Use environment variables (`VITE_CESIUM_ION_TOKEN` in `.env`)

### 6.4 Verification

After implementing, verify with:
```bash
# Check SharedArrayBuffer is available
console.log(typeof SharedArrayBuffer);  // → "function"

# Check no COEP errors
# No "Specify a Cross-Origin Embedder Policy" errors in console

# Check Cesium workers loading
# Network tab should show: /cesium/Workers/*.js → 200 OK

# Check Ion API working
# Console should show: "✓ Ion imagery provider created" (< 2 seconds)
```

**Full Guide**: See `docs/CESIUM_GUIDE.md` - Section 4 "COOP/COEP Integration for SAB"

## 7. Common Pitfalls & Quick Fixes

| Symptom | Logs/Console | Root Cause | Fix |
|---------|--------------|------------|-----|
| **Invisible Globe (Stars Visible)** | `imageryLayers.length = 0` | Provider not added to viewer | Follow Section 3 setup steps; ensure provider passed to constructor; verify fallback adds layer |
| **COEP Blocks Cesium Workers** | "Specify a Cross-Origin Embedder Policy" for `createVerticesFromHeightmap.js` | Worker scripts from node_modules | Configure `vite-plugin-static-copy` per Section 6.2 |
| **Ion API Timeout (5+ seconds)** | "⏳ Waiting for imagery provider to become ready... ⚠ Provider not ready after 5s timeout" | COEP blocking api.cesium.com | Add `/cesium-ion-api` proxy per Section 6.2 |
| **Black Globe with COEP** | Both worker errors AND timeout | Missing complete COEP fix | Implement all steps in Section 6 (asset copy + proxy + config) |
| **WASM HEAPU8 Undefined** | "Cannot read properties of undefined (reading 'set')" | Missing WASM exports | Export `HEAPU8, HEAP8, etc.` in `-sEXPORTED_RUNTIME_METHODS`; call `createNegentropic(Module)` after eval(glue) |
| **deck.gl Worker Errors** | "ResizeObserver undefined", "getBoundingClientRect not a function" | Missing DOM polyfills in worker | Polyfill ResizeObserver/IntersectionObserver; add `offscreen.getBoundingClientRect = () => ({width: 1024, height: 1024})` |
| **OffscreenCanvas Transfer Fail** | "Cannot transfer control more than once" | Multiple transfer attempts | Transfer **once** with transferable flag; create new canvas on error/reconnect |
| **Slow/Blank Mobile** | High zoom, no tiles | Device limitations | Set `maximumLevel: 14`; enable WebGL2 in browser flags; use `requestRenderMode: true` for throttled FPS |
| **Click Works, No Sim** | "WASM_READY" but no plume | SAB sync issue | Check SAB notify: `Atomics.notify(sabView, 0)`; verify offset in Float32Array |
| **Token Errors** | Ion authentication failure | Missing or invalid token | Ensure `.env` file has valid `VITE_CESIUM_ION_TOKEN` |
| **Assets 404** | `Workers/*.js` not found | `CESIUM_BASE_URL` not set | Verify `(window as any).CESIUM_BASE_URL = '/cesium/';` at top of main.ts |

## 8. Future-Proof Extensions
- **WebGPU**: Replace WebGL2 with `navigator.gpu` in render worker (2026 target).
- **Multiplayer**: WebRTC DataChannel for SAB sync between peers.
- **Desktop**: Package with Tauri/Electron → native Cesium for Unreal performance.

---

This guide lives in `/docs/INTERCONNECTION_GUIDE.md`.
Update it every sprint.

**Last Updated**: 2025-11-15 (Complete COEP-compatible architecture validated and operational)

**Recent Changes (Nov 2025 - Production Validated):**
- ✅ **Complete COEP Fix**: Implemented local Cesium asset serving + Ion API proxy
- ✅ **Three-Thread Architecture**: Main → Core Worker → Render Worker fully operational
- ✅ **SharedArrayBuffer**: Working under strict COEP/COOP headers
- ✅ **Cesium Globe Rendering**: Bing Maps imagery loading in < 2 seconds (no timeout)
- ✅ **Zero COEP Errors**: All worker scripts and API calls appear as same-origin
- ✅ **Production Dependencies**: Removed `vite-plugin-cesium`, using `vite-plugin-static-copy`
- ✅ **Secure Token Handling**: Environment variables for Ion API access
- ✅ **Documentation**: Complete troubleshooting guide with root cause analysis

**Architecture Status:**
- Main Thread: CesiumJS globe rendering at 60 FPS ✅
- Core Worker: WASM loader + message interface ready ✅
- Render Worker: deck.gl OffscreenCanvas initialization validated ✅
- SAB + Atomics: Zero-copy memory working ✅

**We now have a living, breathing Earth in the browser.**

**Next: Click the planet and watch it heal.**
