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
  baseLayerPicker: false,  // Disable UI picker (use for single layer)
  baseLayer: true,  // Enable base layer (default: true; false = invisible globe!)

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
- **`baseLayerPicker: false`**: Disables UI picker but keeps base layer functionality. DO NOT set `baseLayer: false` - it hides the globe!
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
| `imageryLayers.length = 0` | No base layer configured | Verify `imageryProvider` is set and `baseLayer` is not false |
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

### Cesium-Specific Setup

Serve Cesium **locally** (not from CDN) to match origin:

```bash
npm install cesium
```

```typescript
import * as Cesium from 'cesium';  // Local bundle (same-origin)
import 'cesium/Build/Cesium/Widgets/widgets.css';
```

Copy Cesium static assets to `public/`:

```typescript
// vite.config.ts
import { defineConfig } from 'vite';
import { viteStaticCopy } from 'vite-plugin-static-copy';

export default defineConfig({
  plugins: [
    viteStaticCopy({
      targets: [
        {
          src: 'node_modules/cesium/Build/Cesium/Workers',
          dest: 'cesium',
        },
        {
          src: 'node_modules/cesium/Build/Cesium/ThirdParty',
          dest: 'cesium',
        },
        {
          src: 'node_modules/cesium/Build/Cesium/Assets',
          dest: 'cesium',
        },
        {
          src: 'node_modules/cesium/Build/Cesium/Widgets',
          dest: 'cesium',
        },
      ],
    }),
  ],
});
```

Set Cesium base URL:

```typescript
import { buildModuleUrl } from 'cesium';

buildModuleUrl.setBaseUrl('/cesium/');
```

### Troubleshooting COEP

| Issue | Cause | Fix |
|-------|-------|-----|
| Blocked `createVerticesFromHeightmap.js` | Worker script from CDN | Serve Cesium locally |
| Iframe embed fails | Child iframe lacks COEP | Add headers to iframe src |
| `SharedArrayBuffer` undefined | COEP/COOP not set | Verify headers in Network tab |

**Test SAB**: `console.log(typeof SharedArrayBuffer)` → should be `'function'`, not `'undefined'`

**Full Guide**: [web.dev COOP/COEP](https://web.dev/articles/coop-coep)

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

## Implementation in GEO-v1

See `web/src/main.ts:141-281` for our production implementation:

- **Viewer setup**: Lines 154-173
- **Globe hardening**: Lines 179-197
- **Critical checks**: Lines 206-211
- **Ready verification**: Lines 227-239

---

## Additional Resources

- **Official Docs**: [Cesium Docs](https://cesium.com/learn/cesiumjs-ref-doc/)
- **API Reference**: [Viewer](https://cesium.com/learn/ion-sdk/ref-doc/Viewer.html) | [ImageryProvider](https://cesium.com/learn/ion-sdk/ref-doc/ImageryProvider.html)
- **Sandcastle**: [Interactive Examples](https://sandcastle.cesium.com/)
- **Community**: [Cesium Forum](https://community.cesium.com/)

---

**Attribution**: This guide synthesizes Cesium v1.120+ documentation with fixes from Grok (xAI), community forums, and production debugging in the Negentropic-Core GEO-v1 pipeline.

**Last Updated**: 2025-11-15
