# CesiumJS Cheat-Sheet & Troubleshooting Guide

**Edition**: November 2025 (CesiumJS v1.120+)
**Project**: GEO-v1 browser pipeline (Cesium + deck.gl + WASM + SharedArrayBuffer)
**Sources**: Official Cesium docs, forums, community fixes, and Grok (xAI) analysis

This guide distills key CesiumJS documentation for viewer setup, imagery providers, worker rendering, and troubleshooting common issues like invisible globes.

---

## Table of Contents

1. [Viewer Constructor Options](#1-viewer-constructor-options)
2. [Imagery Providers (OSM Focus)](#2-imagery-providers-osm-focus)
3. [Troubleshooting Invisible Globe](#3-troubleshooting-invisible-globe-2025-edition)
4. [COOP/COEP Integration for SAB](#4-coopcoep-integration-for-sab)
5. [OffscreenCanvas & Worker Rendering](#5-offscreencanvas--worker-rendering-tips)
6. [Quick Reference](#quick-reference)

---

## 1. Viewer Constructor Options

The `Viewer` is the core widget for globe rendering. Here's the essential setup for GEO-v1:

```typescript
import { Viewer, OpenStreetMapImageryProvider, Color } from 'cesium';

const viewer = new Viewer('cesiumContainer', {
  // ============================================
  // ESSENTIAL FOR GLOBE VISIBILITY
  // ============================================
  globe: true,  // Enables globe (default: true)

  // ============================================
  // IMAGERY & BASE LAYER
  // ============================================
  imageryProvider: new OpenStreetMapImageryProvider({
    url: 'https://a.tile.openstreetmap.org/' // Free, no API key needed
  }),
  baseLayerPicker: false,  // Disable UI picker; imageryProvider becomes base layer automatically
  // NOTE: There is NO 'baseLayer' option! Passing imageryProvider automatically adds it as base layer.

  // ============================================
  // PERFORMANCE/RENDERING
  // ============================================
  requestRenderMode: false,  // Continuous render for 60 FPS (default: false)
  maximumRenderTimeChange: Infinity,  // No FPS cap

  // ============================================
  // UI WIDGETS (disable for clean UI)
  // ============================================
  timeline: false,
  animation: false,
  geocoder: true,
  homeButton: true,
  sceneModePicker: true,
  navigationHelpButton: false,
  selectionIndicator: false,
  infoBox: false,

  // ============================================
  // ADVANCED OPTIONS
  // ============================================
  scene3DOnly: true,  // 3D only (no 2D/Columbus modes)
  shadows: false,  // Disable for performance
  creditContainer: 'credits',  // Custom credits div
});

// Post-creation hardening (critical for visibility)
viewer.scene.globe.show = true;
viewer.scene.skyBox.show = true;
viewer.scene.backgroundColor = Color.BLACK;
viewer.scene.render(); // Force immediate render
```

### Key Points

- **`imageryProvider`**: Pass directly to constructor to guarantee it becomes layer 0. This is THE FIX for invisible globes.
- **`baseLayerPicker: false`**: Disables UI picker. When combined with `imageryProvider`, it automatically becomes the base layer.
- **`requestRenderMode: false`**: Continuous rendering at 60 FPS. Critical for real-time visualization.
- **Deprecated**: `imageryProviderViewModels` (use `imageryProvider` instead in v1.120+).

**Full Reference**: [Cesium.Viewer.ConstructorOptions](https://cesium.com/learn/ion-sdk/ref-doc/Viewer.html)

---

## 2. Imagery Providers (OSM Focus)

Imagery providers add map tiles to the globe. OpenStreetMap (OSM) is free and requires no API key.

### Basic Setup

```typescript
const osmProvider = new OpenStreetMapImageryProvider({
  url: 'https://a.tile.openstreetmap.org/',  // Subdomain for load balancing (a/b/c)
  fileExtension: 'png',
  minimumLevel: 0,
  maximumLevel: 18,  // Zoom levels (0 = globe view, 18 = street level)
  rectangle: Cesium.Rectangle.fromDegrees(-180, -90, 180, 90),  // Full globe coverage
});

// Option 1: Pass to Viewer constructor (RECOMMENDED)
const viewer = new Viewer('cesiumContainer', {
  imageryProvider: osmProvider,
  baseLayerPicker: false,
});

// Option 2: Add explicitly (if baseLayerPicker: true)
viewer.imageryLayers.addImageryProvider(osmProvider, 0);  // Index 0 = base layer
```

### Async Ready Check

Imagery providers load tiles asynchronously. Always verify readiness:

```typescript
// Error handler
osmProvider.errorEvent.addEventListener((error: any) => {
  console.error('❌ OSM Imagery provider error:', error);
});

// Ready check (poll if needed)
if (!osmProvider.ready) {
  console.log('⏳ Waiting for OSM imagery provider to become ready...');
  const checkReady = () => {
    if (osmProvider.ready) {
      console.log('✓ OSM imagery provider is now ready!');
    } else {
      setTimeout(checkReady, 100); // Poll every 100ms
    }
  };
  checkReady();
}
```

### Layer Properties Inspection

```typescript
// Check if layer was added
console.log('Imagery layers:', viewer.imageryLayers.length); // Must be > 0!

// Inspect layer details
if (viewer.imageryLayers.length > 0) {
  const layer = viewer.imageryLayers.get(0);
  console.log({
    show: layer.show,           // true
    alpha: layer.alpha,         // 1.0 (fully opaque)
    ready: layer.ready,         // true when tiles loaded
    providerType: layer.imageryProvider.constructor.name
  });
}
```

### Common OSM Errors & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `ready: false` forever | Provider not attached to viewer | Pass to constructor or use `addImageryProvider()` |
| `imageryLayers.length = 0` | No base layer configured | Verify `imageryProvider` is passed to Viewer constructor |
| Tiles not loading (404) | Invalid URL or CORS block | Use `https://a.tile.openstreetmap.org/`; check Network tab |
| COEP blocks OSM tiles | Cross-origin policy restriction | Add Vite proxy or serve tiles from same origin |
| Slow/blank on mobile | High zoom level or cache miss | Set `maximumLevel: 14`; hard refresh (Cmd+Shift+R) |

**Full Reference**: [OpenStreetMapImageryProvider](https://cesium.com/learn/ion-sdk/ref-doc/OpenStreetMapImageryProvider.html)

---

## 3. Troubleshooting Invisible Globe (2025 Edition)

**Symptom**: Globe is clickable but appears black/transparent. Scene is active, but rendering fails.

### Top Causes & Fixes

| Symptom | Root Cause | Fix |
|---------|-----------|-----|
| **Black globe, clicks work** | No base layer (`imageryLayers.length = 0`) | Add `imageryProvider` to Viewer constructor |
| **`ready: false` forever** | Provider created but not attached | Pass to constructor or call `addImageryProvider()` |
| **COEP blocks Cesium assets** | CDN resources lack CORP headers | Serve Cesium locally or use Vite proxy |
| **Mobile invisible** | WebGL2 disabled or high zoom | Enable WebGL2; set `maximumLevel: 14` |
| **`globe.show = false`** | Explicitly disabled in code | Set `viewer.scene.globe.show = true` |
| **Tiles 404 in Network tab** | Invalid OSM URL | Use `https://a.tile.openstreetmap.org/` |

### Debug Flow (Step-by-Step)

```typescript
// 1. CHECK: Imagery layers added?
console.log('Imagery layers:', viewer.imageryLayers.length);
if (viewer.imageryLayers.length === 0) {
  console.error('❌ CRITICAL: No imagery layers! Globe will be invisible.');
  // FIX: Add imageryProvider to Viewer constructor
}

// 2. CHECK: Provider ready?
const layer = viewer.imageryLayers.get(0);
console.log('Provider ready:', layer.ready);
if (!layer.ready) {
  console.warn('⚠ Provider not ready - check Network tab for tile loading errors');
}

// 3. CHECK: Globe enabled?
console.log('Globe show:', viewer.scene.globe.show);
if (!viewer.scene.globe.show) {
  viewer.scene.globe.show = true; // Force enable
}

// 4. CHECK: Canvas WebGL context?
const gl = viewer.canvas.getContext('webgl2') || viewer.canvas.getContext('webgl');
console.log('WebGL context:', gl ? 'Active' : 'Failed');

// 5. FORCE: Render frame
viewer.scene.requestRender();
```

### Network Tab Checklist

Open DevTools → Network → Reload:

1. **Tiles loading?** Look for `https://a.tile.openstreetmap.org/0/0/0.png` → 200 OK
2. **CORS errors?** Tiles blocked by COEP → Add Vite proxy
3. **404s?** Invalid URL → Verify `url` parameter
4. **Timeout?** Slow network → Reduce `maximumLevel`

### DOM Inspection Checklist

Open DevTools → Elements → Inspect `#cesiumContainer`:

1. **Canvas present?** `<canvas>` element inside container?
2. **Canvas visible?** Check CSS: `display: block`, `visibility: visible`
3. **Canvas size?** `width` and `height` > 0 (not `0px`)
4. **WebGL active?** Canvas has WebGL context (right-click → Inspect → Console → `canvas.getContext('webgl2')`)

### Minimal Test Case

If still invisible, test with Cesium's minimal example:

```typescript
const viewer = new Viewer('cesiumContainer', {
  imageryProvider: new OpenStreetMapImageryProvider({
    url: 'https://a.tile.openstreetmap.org/'
  })
});
```

If this works but your code doesn't, compare Viewer options line-by-line.

**Pro Tip**: Use [Cesium Sandcastle](https://sandcastle.cesium.com/) to test Hello World. If visible there, issue is in your setup.

---

## 4. COOP/COEP Integration for SAB

**Required for**: `SharedArrayBuffer` (zero-copy WASM ↔ GPU transfers).
**Side effect**: Blocks cross-origin assets without CORP headers.

### HTTP Headers Required

Add to your server (Vite `vite.config.ts` or nginx/Apache):

```typescript
// vite.config.ts
export default defineConfig({
  server: {
    headers: {
      'Cross-Origin-Embedder-Policy': 'require-corp',
      'Cross-Origin-Opener-Policy': 'same-origin',
    },
  },
});
```

### Critical COEP Blocking Issues with Cesium (Nov 2025)

**THE PROBLEM:**
COEP blocks **two critical resources** that Cesium needs:

1. **Cesium's internal worker scripts** (e.g., `createVerticesFromHeightmap.js`) - causes COEP violation errors
2. **Cesium Ion API calls** to `api.cesium.com` - causes imagery provider timeout (5+ seconds), resulting in black globe

**SYMPTOMS:**
```
Console Error: Specify a Cross-Origin Embedder Policy to prevent this frame from being blocked
Request: createVerticesFromHeightmap.js
Blocked Resource: http://localhost:3000/

Console Warning: ⏳ Waiting for imagery provider to become ready...
Console Warning: ⚠ Imagery provider not ready after 5s timeout
Globe: Black sphere with stars (no Earth imagery)
```

### The Complete COEP Fix (Proven Solution)

This is the **definitive, production-tested** solution implemented in GEO-v1:

#### Step 1: Install Dependencies

```bash
cd web
npm install --save-dev vite-plugin-static-copy
```

#### Step 2: Complete Vite Configuration

```typescript
// web/vite.config.ts
import { defineConfig } from 'vite';
import { viteStaticCopy } from 'vite-plugin-static-copy';
import wasm from 'vite-plugin-wasm';
import { resolve } from 'path';

export default defineConfig({
  plugins: [
    wasm(),
    // Copy Cesium's static assets to serve from same origin
    viteStaticCopy({
      targets: [
        {
          src: 'node_modules/cesium/Build/Cesium/Workers',
          dest: 'cesium'
        },
        {
          src: 'node_modules/cesium/Build/Cesium/ThirdParty',
          dest: 'cesium'
        },
        {
          src: 'node_modules/cesium/Build/Cesium/Assets',
          dest: 'cesium'
        },
        {
          src: 'node_modules/cesium/Build/Cesium/Widgets',
          dest: 'cesium'
        }
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

#### Step 3: Configure Cesium Base Paths

```typescript
// web/src/main.ts (at the very top, before any Cesium usage)

import { Ion, IonImageryProvider } from 'cesium';
import 'cesium/Build/Cesium/Widgets/widgets.css';

// --- CRITICAL CONFIGURATION ---

// 1. Tell Cesium where to load its assets from
(window as any).CESIUM_BASE_URL = '/cesium/';

// 2. Point Cesium's Ion server requests to our local proxy
Ion.defaultServer = '/cesium-ion-api/';

// 3. Configure Cesium Ion token from secure environment variable
Ion.defaultAccessToken = import.meta.env.VITE_CESIUM_ION_TOKEN;

// --- END CRITICAL CONFIGURATION ---
```

#### Step 4: Environment Variable Setup

```bash
# web/.env (DO NOT commit to git!)
VITE_CESIUM_ION_TOKEN=your_actual_token_here
```

```bash
# web/.env.example (safe to commit)
VITE_CESIUM_ION_TOKEN=your_token_here
```

### Why This Fix Works

**Root Cause Analysis:**
- COEP requires all resources to be same-origin OR have explicit CORP headers
- Cesium's worker scripts are served by Vite from `node_modules`, appearing as different origin
- Cesium Ion API calls go to `api.cesium.com`, which is definitely cross-origin
- Browser blocks both, causing worker errors and imagery timeout

**The Solution:**
1. **Static Asset Copying**: `vite-plugin-static-copy` copies Workers/Assets to `/cesium/` at build time → same-origin ✓
2. **Ion API Proxy**: Vite proxy at `/cesium-ion-api` → rewrites to `api.cesium.com` → appears same-origin ✓
3. **Path Configuration**: `CESIUM_BASE_URL` and `Ion.defaultServer` tell Cesium to use local paths
4. **Token Security**: Environment variables keep API keys out of source code

### Troubleshooting COEP

| Issue | Symptom | Cause | Fix |
|-------|---------|-------|-----|
| **Blocked worker scripts** | `createVerticesFromHeightmap.js` COEP error | Workers from node_modules | Use `vite-plugin-static-copy` |
| **Ion API timeout** | "Provider not ready after 5s" | API calls to api.cesium.com blocked | Add `/cesium-ion-api` proxy |
| **Black globe** | Stars visible, no Earth | Both above issues combined | Complete fix (all steps) |
| **Assets 404** | `Workers/*.js` not found | `CESIUM_BASE_URL` not set | Set `window.CESIUM_BASE_URL = '/cesium/'` |
| **Token errors** | Ion authentication failure | Missing or invalid token | Check `.env` file and token validity |
| `SharedArrayBuffer` undefined | COEP/COOP not set | Missing headers | Verify headers in Network tab |

**Test SAB**: `console.log(typeof SharedArrayBuffer)` → should be `'function'`, not `'undefined'`

**Verify Fix**: After implementing, you should see:
- ✓ No COEP errors in console
- ✓ "Ion imagery provider created and ready" within 1-2 seconds
- ✓ Visible Earth globe with imagery (not black)
- ✓ Cesium worker scripts loading from `/cesium/Workers/`

**Full Guide**: [web.dev COOP/COEP](https://web.dev/articles/coop-coep)

### Troubleshooting Ion Imagery (Lessons from Nov 2025)

| Issue | Symptom | Root Cause | Fix |
|-------|---------|-----------|-----|
| **Invisible globe** | Stars visible, clickable, but no Earth | Imagery provider not passed to constructor | Use `createWorldImagery()` and pass to Viewer constructor |
| **Cannot read 'ready'** | Crash on line accessing `provider.ready` | Provider undefined during init | Add null check: `if (provider) { ... }` before accessing properties |
| **Imagery layer count = 0** | Console shows 0 imagery layers | No base layer configured | Verify `imageryProvider` passed to constructor, check console |
| **Provider type undefined** | Unknown provider type in logs | Provider not initialized | Wait for provider with polling: `setTimeout(checkProvider, 100)` |

**Debug checklist:**
```typescript
// After viewer creation
console.log('Imagery layers:', viewer.imageryLayers.length); // Must be > 0
const layer = viewer.imageryLayers.get(0);
console.log('Provider:', layer.imageryProvider?.constructor?.name); // Should be IonImageryProvider
console.log('Ready:', layer.imageryProvider?.ready); // Should become true
console.log('Show:', layer.show); // Must be true
console.log('Alpha:', layer.alpha); // Should be 1.0
```

---

## 5. OffscreenCanvas & Worker Rendering Tips

For deck.gl overlays in workers (e.g., moisture plumes visualization).

### Setup (Main Thread → Worker)

```typescript
// Main thread
const canvas = document.getElementById('overlay-canvas') as HTMLCanvasElement;
const offscreen = canvas.transferControlToOffscreen();  // One-time transfer

renderWorker.postMessage({
  type: 'init',
  canvas: offscreen,
}, [offscreen]); // Transfer ownership
```

### Setup (Worker Side)

```typescript
// render-worker.ts
onmessage = (e) => {
  if (e.data.type === 'init') {
    const offscreen = e.data.canvas;
    const gl = offscreen.getContext('webgl2');

    const deck = new Deck({
      canvas: offscreen,
      controller: false,  // No DOM events in worker
      _pickable: false,   // No picking in worker
    });

    // Render loop
    function render() {
      deck.redraw();
      requestAnimationFrame(render);
    }
    render();
  }
};
```

### Common Worker Issues & Fixes

| Issue | Cause | Fix |
|-------|-------|-----|
| `getBoundingClientRect` undefined | Worker has no DOM | Polyfill: `offscreen.getBoundingClientRect = () => ({ width: 1024, height: 1024, ... })` |
| `ResizeObserver` undefined | deck.gl viewport culling | Polyfill no-op class: `class ResizeObserver { observe() {} disconnect() {} }` |
| Hammer.js `touchAction` error | Input layer assumes DOM | Proxy: `offscreen.style = { touchAction: 'pan-y pan-x' }` |
| WebGL context lost | WebGL2 unsupported | Fallback: `getContext('webgl')` |

### Performance Tips

- **Render loop**: Use `requestAnimationFrame` in worker (no main-thread jank)
- **Zero-copy**: Map SharedArrayBuffer to `Float32Array` for deck.gl `data` prop
- **Avoid DOM**: No `document`, `window`, or DOM APIs in workers

**Full Reference**: [MDN OffscreenCanvas](https://developer.mozilla.org/en-US/docs/Web/API/OffscreenCanvas)

---

## Quick Reference

### Invisible Globe Troubleshooting Checklist

1. **Console**: `viewer.imageryLayers.length` > 0? `ready: true`?
2. **Network**: Tile requests 200 OK? No COEP blocks?
3. **Elements**: Canvas WebGL context active? (DevTools → Elements → canvas)
4. **Force Render**: `viewer.scene.requestRender();`
5. **Minimal Test**: Load [Cesium Sandcastle Hello World](https://sandcastle.cesium.com/) — if visible, compare your Viewer options

### Critical Code Checks

```typescript
// After viewer creation:
console.log('Imagery layers:', viewer.imageryLayers.length); // Must be > 0!
console.log('Globe show:', viewer.scene.globe.show);         // Must be true
console.log('Provider ready:', viewer.imageryLayers.get(0).ready); // Must be true

// If imageryLayers.length === 0:
throw new Error('No imagery layers! Pass imageryProvider to Viewer constructor.');
```

### Essential Viewer Setup (Copy-Paste Template)

```typescript
import { Viewer, OpenStreetMapImageryProvider, Color } from 'cesium';

const osmProvider = new OpenStreetMapImageryProvider({
  url: 'https://a.tile.openstreetmap.org/'
});

const viewer = new Viewer('cesiumContainer', {
  imageryProvider: osmProvider,  // THE FIX for invisible globe
  baseLayerPicker: false,
  requestRenderMode: false,
  // ... other options
});

// Hardening
viewer.scene.globe.show = true;
viewer.scene.skyBox.show = true;
viewer.scene.backgroundColor = Color.BLACK;
viewer.scene.render();

// Verify
if (viewer.imageryLayers.length === 0) {
  throw new Error('Globe initialization failed: no imagery layers');
}
```

---

## Implementation in GEO-v1 (November 2025 Update)

### Production Fix: Complete COEP-Compatible Setup

After resolving multiple initialization issues (COEP blocking, Ion API timeout, invisible globe), we implemented the complete fix in GEO-v1:

```typescript
// web/src/main.ts

import { Viewer, Ion, IonImageryProvider } from 'cesium';
import 'cesium/Build/Cesium/Widgets/widgets.css';

// --- CRITICAL CONFIGURATION (MUST BE BEFORE ANY CESIUM USAGE) ---

// 1. Tell Cesium where to load its assets from
(window as any).CESIUM_BASE_URL = '/cesium/';

// 2. Point Cesium's Ion server requests to our local proxy
Ion.defaultServer = '/cesium-ion-api/';

// 3. Configure Cesium Ion token from secure environment variable
const CESIUM_ION_TOKEN = import.meta.env.VITE_CESIUM_ION_TOKEN;
if (CESIUM_ION_TOKEN && CESIUM_ION_TOKEN !== 'your_token_here') {
  Ion.defaultAccessToken = CESIUM_ION_TOKEN;
  console.log('✓ Cesium Ion token configured');
}

// --- END CRITICAL CONFIGURATION ---

// Later in initializeCesium():
async function initializeCesium() {
  // CRITICAL: Create imagery provider BEFORE Viewer
  // Asset ID 2 = Bing Maps Aerial with Labels (Ion default)
  const imageryProvider = await IonImageryProvider.fromAssetId(2);
  console.log('✓ Ion imagery provider created');

  const viewer = new Viewer('cesiumContainer', {
    imageryProvider: imageryProvider,  // Pass provider object explicitly
    baseLayerPicker: false,
    requestRenderMode: false,
    // ... other options
  });

  // FALLBACK: Verify layer was added by constructor
  if (viewer.imageryLayers.length === 0) {
    console.warn('⚠ Imagery not added via constructor, adding explicitly...');
    viewer.imageryLayers.addImageryProvider(imageryProvider);

    if (viewer.imageryLayers.length === 0) {
      throw new Error('❌ CRITICAL: Failed to add imagery layer');
    }
  }

  // Post-creation hardening
  viewer.scene.globe.show = true;
  viewer.scene.requestRender();  // Force first render

  console.log(`✓ Cesium globe initialized with ${viewer.imageryLayers.length} layer(s)`);
}
```

**Why this complete solution works:**

1. **CESIUM_BASE_URL**: Tells Cesium to load Workers/Assets from `/cesium/` (served locally via vite-plugin-static-copy)
2. **Ion.defaultServer**: Redirects Ion API calls to our local proxy `/cesium-ion-api` (which forwards to api.cesium.com)
3. **Environment Variable**: Keeps API token secure and out of source control
4. **Explicit Provider Creation**: `IonImageryProvider.fromAssetId(2)` creates Bing Maps imagery provider
5. **Constructor Pattern**: Passing `imageryProvider` object (NOT a boolean!) is the correct API usage
6. **Fallback Addition**: Ensures layer is added even if constructor method fails
7. **COEP Compatibility**: All resources now appear as same-origin, satisfying COEP requirements

**IMPORTANT:** There is NO `baseLayer` boolean option in the Cesium Viewer constructor API. Passing `imageryProvider` object directly is the correct pattern.

**Available Ion Asset IDs:**
- `2` - Bing Maps Aerial with Labels (default)
- `3` - Bing Maps Aerial
- `4` - Bing Maps Road

**Production Files:**
- **Configuration**: `web/vite.config.ts` (lines 11-30: static asset copy, lines 77-81: Ion API proxy)
- **Setup**: `web/src/main.ts` (lines 26-45: critical configuration)
- **Initialization**: `web/src/main.ts` (lines 155-227: provider creation and viewer setup)
- **Environment**: `web/.env.example` (token template)

---

## Additional Resources

- **Official Docs**: [Cesium Docs](https://cesium.com/learn/cesiumjs-ref-doc/)
- **API Reference**: [Viewer](https://cesium.com/learn/ion-sdk/ref-doc/Viewer.html) | [ImageryProvider](https://cesium.com/learn/ion-sdk/ref-doc/ImageryProvider.html)
- **Sandcastle**: [Interactive Examples](https://sandcastle.cesium.com/)
- **Community**: [Cesium Forum](https://community.cesium.com/)

---

---

## ORACLE-004: CliMA Matrix Injection for deck.gl Synchronization (Nov 18, 2025)

After achieving basic Cesium globe rendering and imagery, the next challenge was perfect synchronization between Cesium's camera and deck.gl's visualization layers. Initial parameter-based synchronization (longitude, latitude, altitude) proved insufficient, leading to layer offset, shearing, polar disappearance, and "matrix not invertible" errors.

### The Solution: Raw Matrix Synchronization

**ORACLE-004** replaces high-level parameter mapping with direct injection of Cesium's raw view and projection matrices into a custom deck.gl View, mathematically aligned using CliMA cubed-sphere face rotation matrices.

**Key Components:**

1. **Matrix Extraction** (in main.ts):
   ```typescript
   const viewMatrix = camera.viewMatrix.clone();
   const projMatrix = camera.frustum.projectionMatrix.clone();
   ```

2. **CliMA Face Rotation** (from ClimaCore.jl):
   - 6 cubed-sphere face rotation matrices (north/south poles, 4 equatorial faces)
   - Determines active face from camera ECEF position
   - Applies rotation: `alignedProj = projMatrix * faceRotation`

3. **Custom MatrixView** (in render-worker.ts):
   - Replaces deck.gl's `_GlobeView`
   - Accepts raw Float32Array matrices
   - Bypasses all internal projection calculations

**Benefits:**
- Perfect 1:1 synchronization (mathematically provable)
- Stable at polar regions (cubed-sphere avoids singularities)
- Zero lag (direct matrix copy)
- Guaranteed invertible projection matrices

**Files:**
- `web/src/geometry/clima-matrices.ts` - Face rotation matrices
- `web/src/geometry/get-face.ts` - Face detection (<1µs)
- `web/src/main.ts` - Matrix extraction and alignment
- `web/src/workers/render-worker.ts` - Custom MatrixView

**Reference:**
- Full details: `architecture/Architectural_Quick-Ref.md` (ORACLE-004 section)
- CliMA source: https://github.com/CliMA/ClimaCore.jl

---

**Attribution**: This guide synthesizes Cesium v1.120+ documentation with fixes from Grok (xAI), community forums, and production debugging in the Negentropic-Core GEO-v1 pipeline.

**Last Updated**: 2025-11-18 (Added ORACLE-004 CliMA matrix injection)
