# GEO-v1 Development Environment Setup

**Edition**: November 2025
**Status**: Production-Validated Configuration
**Last Updated**: 2025-11-15

This document provides the complete, step-by-step setup for the negentropic-core GEO-v1 web interface.

---

## Prerequisites

### Required Software

| Tool | Minimum Version | Purpose | Installation |
|------|----------------|---------|--------------|
| **Node.js** | 18.0.0+ | JavaScript runtime | [nodejs.org](https://nodejs.org/) |
| **npm** | 9.0.0+ | Package manager | Included with Node.js |
| **Emscripten SDK** | 4.0.19+ | WASM compilation | [emscripten.org](https://emscripten.org/docs/getting_started/downloads.html) |
| **CMake** | 3.20+ | Build system | [cmake.org](https://cmake.org/download/) |
| **Git** | 2.30+ | Version control | [git-scm.com](https://git-scm.com/) |

### Operating System Support

- ✅ **Linux**: Fully supported (Ubuntu 20.04+, Debian 11+)
- ✅ **macOS**: Fully supported (macOS 11+)
- ✅ **Windows**: Supported via WSL2

---

## Quick Start (5 Minutes)

```bash
# 1. Clone repository
git clone https://github.com/dj-ccs/negentropic-core.git
cd negentropic-core

# 2. Install Emscripten SDK (if not already installed)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 4.0.19
./emsdk activate 4.0.19
source ./emsdk_env.sh
cd ..

# 3. Navigate to web directory
cd web

# 4. Install dependencies (skip optional packages to avoid build errors)
npm install --ignore-scripts

# 5. Create environment file
cp .env.example .env

# 6. Add your Cesium Ion token to .env
# Get free token at: https://cesium.com/ion/tokens
# Edit .env and replace: VITE_CESIUM_ION_TOKEN=your_actual_token_here

# 7. Build WASM module
./build-wasm.sh

# 8. Start development server
npm run dev

# 9. Open browser to http://localhost:3000
```

**Expected Result**: Cesium globe rendering with Bing Maps imagery in < 2 seconds.

---

## Detailed Setup Instructions

### Step 1: Emscripten SDK Installation

Emscripten is required to compile C++ code to WebAssembly.

```bash
# Clone Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate specific version
./emsdk install 4.0.19
./emsdk activate 4.0.19

# Configure environment (add to ~/.bashrc or ~/.zshrc for persistence)
source ./emsdk_env.sh

# Verify installation
emcc --version
# Should show: emcc (Emscripten gcc/clang-like replacement) 4.0.19
```

**Troubleshooting**:
- If `emcc` not found, ensure `source ./emsdk_env.sh` was executed
- On macOS, may need to install Xcode Command Line Tools: `xcode-select --install`

### Step 2: Node.js Dependencies

```bash
cd negentropic-core/web

# Install all packages
# NOTE: Use --ignore-scripts to avoid optional dependency build failures (sharp, etc.)
npm install --ignore-scripts

# Verify installation
npm list vite vite-plugin-static-copy vite-plugin-wasm cesium
```

**Package Overview**:
- **cesium** (^1.120.0): 3D globe rendering engine
- **vite** (^5.3.1): Fast development server and build tool
- **vite-plugin-static-copy** (^1.0.6): Copy Cesium assets for COEP compliance
- **vite-plugin-wasm** (^3.3.0): WebAssembly module support
- **@xenova/transformers** (^2.17.0): Prithvi-100M inference
- **deck.gl** (^9.0.0): WebGL visualization overlays

**Troubleshooting**:
- If `sharp` build fails, ignore it - we removed `vite-plugin-cesium` which was the only package requiring it
- If other packages fail, try: `rm -rf node_modules package-lock.json && npm install --ignore-scripts`

### Step 3: Environment Configuration

```bash
# Create local environment file
cp .env.example .env

# Edit .env and add your Cesium Ion token
nano .env  # or use your preferred editor
```

**Required Environment Variables**:

```bash
# web/.env
VITE_CESIUM_ION_TOKEN=your_actual_token_here
```

**Getting Your Cesium Ion Token**:
1. Go to [https://cesium.com/ion/tokens](https://cesium.com/ion/tokens)
2. Sign up for free account (no credit card required)
3. Create new token with scopes: `assets:read`, `geocode`
4. Copy token and paste into `.env` file

**Security Notes**:
- ✅ `.env` is in `.gitignore` - safe to add secrets
- ❌ Never commit `.env` to git
- ❌ Never hard-code tokens in source files
- ✅ Use `.env.example` as template (committed to git)

### Step 4: WASM Module Build

```bash
# Ensure Emscripten environment is active
source ../emsdk/emsdk_env.sh

# Build WebAssembly module
./build-wasm.sh

# Verify build artifacts
ls -lh public/wasm/
# Should show:
# - negentropic_core.wasm (~500KB - 2MB)
# - negentropic_core.js (glue code)
```

**Build Configuration** (in `build-wasm.sh`):
```bash
emcmake cmake -B build-wasm -DBUILD_WASM=ON
cmake --build build-wasm
cp build-wasm/negentropic_core.wasm web/public/wasm/
cp build-wasm/negentropic_core.js web/public/wasm/
```

**Emscripten Flags Used**:
- `-s MODULARIZE=1`: Export as ES6 module
- `-s EXPORT_ES6=1`: Use ES6 export syntax
- `-s PTHREAD_POOL_SIZE=2`: Worker thread pool
- `-s ALLOW_MEMORY_GROWTH=1`: Dynamic memory allocation
- `-s EXPORTED_RUNTIME_METHODS=['...']`: Expose WASM memory

**Troubleshooting**:
- If build fails with "emcc: command not found", run `source ../emsdk/emsdk_env.sh`
- If CMake errors occur, ensure CMakeLists.txt has `BUILD_WASM` option
- If linker errors occur, check that all C++ dependencies are available

### Step 5: Vite Configuration Verification

The project is pre-configured with COEP-compatible Vite setup. Verify your `vite.config.ts`:

```typescript
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
    // Proxy Cesium Ion API to make it same-origin
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

**Why This Configuration Is Critical**:
1. **COEP/COOP Headers**: Required for SharedArrayBuffer (three-thread architecture)
2. **Static Asset Copy**: Serves Cesium workers from same origin (prevents COEP blocking)
3. **Ion API Proxy**: Makes Ion API calls appear as same-origin (prevents timeout)

---

## Running the Application

### Development Mode

```bash
cd web

# Start development server
npm run dev

# Server starts on http://localhost:3000
# Open in browser (Chrome/Edge recommended for best WebGL2 support)
```

**Expected Console Output**:
```
✓ Cesium Ion token configured
✓ Ion imagery provider created
✓ Cesium Viewer created
✓ Cesium globe initialized with 1 layer(s)
✓ Core Worker script loaded
✓ Core Worker fully initialized (WASM loaded)
✓ Render Worker script loaded
✓ Render Worker fully initialized (deck.gl loaded)
Ready - Click on globe to select region
```

**Performance Targets**:
- **First Paint**: < 1 second
- **Globe Visible**: < 2 seconds
- **Workers Ready**: < 5 seconds
- **Frame Rate**: 60 FPS

### Production Build

```bash
# Build optimized production bundle
npm run build

# Preview production build
npm run preview

# Deploy dist/ directory to hosting service
```

**Build Output** (`web/dist/`):
- `index.html`: Main entry point
- `assets/*.js`: Minified JavaScript bundles
- `assets/*.css`: Minified CSS
- `cesium/`: Cesium static assets (Workers, Assets, Widgets)
- `wasm/`: WASM module and glue code

---

## Verification & Testing

### Browser Compatibility

**Recommended Browsers**:
- ✅ Chrome 90+ (Best performance)
- ✅ Edge 90+ (Chromium-based)
- ✅ Firefox 88+ (Good performance)
- ⚠️ Safari 15+ (Works but slower WebGL)

**Required Browser Features**:
- WebGL2
- SharedArrayBuffer
- OffscreenCanvas
- Web Workers
- ES2022 JavaScript

### Verification Checklist

Open browser console (`F12`) and verify:

```javascript
// 1. SharedArrayBuffer available
console.log(typeof SharedArrayBuffer);
// → "function" (if "undefined", COEP headers not working)

// 2. Check Cesium assets loading
// Network tab should show:
// ✅ /cesium/Workers/*.js → 200 OK
// ✅ /cesium-ion-api/* → 200 OK (proxied to api.cesium.com)

// 3. Check globe rendering
// You should see:
// - Blue Earth sphere (not black)
// - Bing Maps satellite imagery
// - UI controls (geocoder, home button, etc.)

// 4. Check workers
// Console should show:
// ✅ "Core Worker fully initialized (WASM loaded)"
// ✅ "Render Worker fully initialized (deck.gl loaded)"
```

### Common Issues

| Issue | Symptom | Solution |
|-------|---------|----------|
| **Black Globe** | Stars visible, no Earth | Check `.env` has valid token; verify COEP setup in Section 6 of INTERCONNECTION_GUIDE.md |
| **"SharedArrayBuffer is not defined"** | Console error | COEP/COOP headers not set; check vite.config.ts headers |
| **"COEP blocks createVerticesFromHeightmap.js"** | Console warning | `vite-plugin-static-copy` not configured; verify vite.config.ts |
| **"Ion API timeout after 5s"** | Console warning | Ion API proxy missing; add `/cesium-ion-api` proxy in vite.config.ts |
| **WASM not loading** | "Module not found" | Run `./build-wasm.sh`; verify `public/wasm/*.wasm` exists |
| **Workers failing** | Worker errors in console | Check Emscripten flags; verify WASM exports |

---

## Production Deployment

### COEP/COOP Headers (Required)

Your production server **must** send these headers:

**Nginx**:
```nginx
add_header Cross-Origin-Embedder-Policy "require-corp";
add_header Cross-Origin-Opener-Policy "same-origin";
```

**Apache**:
```apache
Header set Cross-Origin-Embedder-Policy "require-corp"
Header set Cross-Origin-Opener-Policy "same-origin"
```

**Cloudflare Workers**:
```javascript
response.headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
response.headers.set('Cross-Origin-Opener-Policy', 'same-origin');
```

### Environment Variables

Create production environment file:
```bash
# web/.env.production
VITE_CESIUM_ION_TOKEN=your_production_token_here
```

Build with production env:
```bash
npm run build
```

### Static Hosting

Compatible with:
- ✅ Cloudflare Pages
- ✅ Vercel
- ✅ Netlify
- ✅ AWS S3 + CloudFront
- ✅ GitHub Pages (with COOP/COEP via Service Worker)

**Note**: Some platforms (GitHub Pages, S3) don't support custom headers. Use a Service Worker to inject COEP/COOP headers.

---

## Troubleshooting

### Reset Everything

If you encounter persistent issues:

```bash
# 1. Clean npm
cd web
rm -rf node_modules package-lock.json
npm install --ignore-scripts

# 2. Rebuild WASM
./build-wasm.sh

# 3. Clear Vite cache
rm -rf node_modules/.vite

# 4. Restart dev server
npm run dev

# 5. Hard refresh browser
# Chrome/Edge: Ctrl+Shift+R (Windows/Linux) or Cmd+Shift+R (Mac)
# Firefox: Ctrl+F5 (Windows/Linux) or Cmd+Shift+R (Mac)
```

### Enable Verbose Logging

Edit `web/src/main.ts`:
```typescript
// At top of file
const DEBUG = true;  // Force debug mode even in production
```

Rebuild and check console for detailed initialization logs.

### Browser Developer Tools

**Network Tab**:
- Filter by "cesium" to see asset loading
- Check for 404s or CORS errors
- Verify Ion API calls are going to `/cesium-ion-api/` (not `api.cesium.com`)

**Console Tab**:
- Look for COEP violation warnings
- Check for worker errors
- Verify "✓" success messages

**Performance Tab**:
- Record page load
- Check for main-thread jank (should be minimal)
- Workers should run in separate threads

---

## Next Steps

After successful setup:

1. **Read Architecture Docs**:
   - `docs/INTERCONNECTION_GUIDE.md` - Three-thread architecture
   - `docs/CESIUM_GUIDE.md` - Cesium integration details
   - `docs/core-architecture.md` - WASM core design

2. **Explore Code**:
   - `web/src/main.ts` - Main thread orchestration
   - `web/src/workers/core-worker.ts` - WASM simulation worker
   - `web/src/workers/render-worker.ts` - deck.gl rendering worker

3. **Run Simulation**:
   - Click on globe to select region
   - Click "Start" to begin simulation
   - Watch real-time visualization

4. **Customize**:
   - Modify simulation parameters in `types/geo-api.ts`
   - Add new visualization layers in `render-worker.ts`
   - Integrate with Prithvi-100M for initial state seeding

---

## Support & Resources

**Documentation**:
- [CesiumJS Official Docs](https://cesium.com/learn/cesiumjs-ref-doc/)
- [Emscripten Guide](https://emscripten.org/docs/)
- [Vite Documentation](https://vitejs.dev/)
- [deck.gl Documentation](https://deck.gl/)

**Community**:
- [Cesium Forum](https://community.cesium.com/)
- [Emscripten GitHub](https://github.com/emscripten-core/emscripten)

**Project Resources**:
- GitHub Repository: `https://github.com/dj-ccs/negentropic-core`
- Issue Tracker: `https://github.com/dj-ccs/negentropic-core/issues`

---

**Last Updated**: 2025-11-15
**Status**: ✅ Production-Validated Configuration
**Maintainer**: dj-ccs

**The Earth is alive in your browser. Let's watch it heal.**
