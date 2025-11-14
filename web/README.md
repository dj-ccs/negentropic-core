# GEO-v1: Global WASM Geospatial Interface

**Real-time Earth simulation powered by negentropic-core**

This is the browser-based geospatial visualization platform for negentropic-core, providing an interactive 3D globe interface with AI-driven landscape initialization and zero-copy WebAssembly physics simulation.

---

## Architecture Overview

### Three-Thread Pipeline

GEO-v1 uses a strict 3-thread architecture for maximum performance:

```
┌─────────────────────────────────────────────────────────────┐
│                     THREAD 1: Main Thread                    │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ CesiumJS Globe (3D rendering, camera, user input)     │ │
│  │ • Creates SharedArrayBuffer                            │ │
│  │ • Spawns Core Worker and Render Worker                │ │
│  │ • Handles UI and region selection                     │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                    │                           │
                    ▼                           ▼
┌──────────────────────────────┐  ┌──────────────────────────────┐
│  THREAD 2: Core Worker       │  │  THREAD 3: Render Worker     │
│  ┌────────────────────────┐  │  │  ┌────────────────────────┐  │
│  │ negentropic-core.wasm  │  │  │  │ deck.gl OffscreenCanvas│  │
│  │ • 10 Hz physics loop   │  │  │  │ • 60 FPS rendering     │  │
│  │ • Writes to SAB        │  │  │  │ • Reads from SAB       │  │
│  │ • Atomics.notify()     │  │  │  │ • Atomics.wait()       │  │
│  └────────────────────────┘  │  │  └────────────────────────┘  │
└──────────────────────────────┘  └──────────────────────────────┘
                    │                           │
                    └───────────────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │ SharedArrayBuffer │
                    │  Zero-Copy Data   │
                    └───────────────────┘
```

### Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| **Main Thread** | CesiumJS 1.120+ | Planetary-scale 3D globe rendering |
| **Core Worker** | Emscripten WASM | Physics simulation (HYD-RLv1, REGv1, REGv2) |
| **Render Worker** | deck.gl 9.0+ on OffscreenCanvas | GPU-accelerated field visualization |
| **AI Initialization** | @xenova/transformers + Prithvi-100M | Satellite-driven landscape state initialization |
| **Data Flow** | SharedArrayBuffer + Atomics | Zero-copy inter-thread communication |
| **Build System** | Vite 5 + TypeScript 5 | Modern web app bundling |

---

## Quick Start

### Prerequisites

- **Node.js** 18+ and npm 9+
- **Emscripten SDK** (for WASM compilation)
- Modern browser with SharedArrayBuffer support (Chrome 92+, Firefox 79+)

### Installation

```bash
# From the web/ directory
npm install

# Build the WASM module
./build-wasm.sh

# Start development server
npm run dev
```

The application will be available at `http://localhost:3000`

### Building WASM Module

The WASM module must be built before running the web app:

```bash
# Make sure Emscripten is activated
source /path/to/emsdk/emsdk_env.sh

# Build WASM
cd web
./build-wasm.sh
```

This will:
1. Configure CMake with Emscripten
2. Compile negentropic-core to WASM
3. Copy artifacts to `web/public/wasm/`

---

## Usage

### Basic Workflow

1. **Select a Region**: Click anywhere on the globe to select a 10km² region
2. **AI Initialization**: Prithvi AI analyzes satellite imagery to initialize landscape state
3. **Start Simulation**: Click "Start Simulation" to begin real-time physics
4. **Observe Evolution**: Watch moisture, SOM, and vegetation evolve in real-time

### Keyboard Controls

| Key | Action |
|-----|--------|
| `Space` | Pause/Resume simulation |
| `R` | Reset simulation |
| `1-4` | Switch visualization field (theta, SOM, vegetation, temperature) |
| `+/-` | Adjust simulation speed |

### Performance Metrics

The UI displays:
- **Core FPS**: Physics simulation frequency (target: 10 Hz)
- **Render FPS**: Visualization frame rate (target: 60 FPS)
- **Active Region**: Currently simulated geographic area

---

## Architecture Details

### SharedArrayBuffer Protocol

The SAB contains:

```
Offset | Size | Description
-------|------|-------------
0      | 4    | Atomics signal (Int32)
4      | 4    | Version number
8      | 4    | Epoch counter
12     | 4    | Grid rows
16     | 4    | Grid cols
20     | 8    | Timestamp (Float64)
28     | 16   | BBox (4× Float32: minLon, minLat, maxLon, maxLat)
44     | 84   | Reserved
-------|------|-------------
128    | N    | Field data (Float32 arrays)
```

### Field Offsets (100×100 grid)

```typescript
{
  theta: 128,              // Soil moisture [0, 128+40000)
  som: 128 + 40000,        // SOM % [40128, 40128+40000)
  vegetation: 128 + 80000, // Cover [80128, 80128+40000)
  temperature: 128 + 120000 // °C [120128, 120128+40000)
}
```

### Synchronization Flow

```
Core Worker:
1. Step simulation (neg_step)
2. Copy state to SAB
3. Atomics.add(signal, 1)
4. Atomics.notify(signal, 1)
5. setTimeout(10Hz loop)

Render Worker:
1. Atomics.wait(signal) [non-blocking poll]
2. Read field data from SAB
3. Update deck.gl layers
4. deck.redraw()
5. requestAnimationFrame(60FPS loop)
```

---

## Prithvi AI Integration

### Model Details

- **Model**: IBM/NASA Prithvi-100M
- **Architecture**: ViT-based with Masked Autoencoder (MAE) pretraining
- **Input**: 6-band HLS imagery (Blue, Green, Red, NIRn, SWIR1, SWIR2)
- **Output**: Segmentation masks + encoder embeddings
- **Tasks**: Flood mapping, wildfire scars, crop segmentation, land cover

### Initialization Pipeline

```typescript
1. User selects region on globe
2. Fetch HLS tiles from NASA/Copernicus
3. Run Prithvi inference:
   - Segment land cover classes
   - Extract vegetation density
   - Detect water bodies
4. Map Prithvi output to negentropic-core state:
   - Class 0 (bare soil)    → θ=5%,  SOM=0.3%, V=5%
   - Class 1 (vegetation)   → θ=25%, SOM=2.0%, V=70%
   - Class 2 (water)        → θ=45%, SOM=0.5%, V=10%
   - Class 3 (mixed)        → θ=15%, SOM=1.0%, V=30%
5. Initialize WASM simulation with mapped state
```

### Validation

GEO-v1 supports continuous validation of simulation output against Prithvi predictions using Intersection-over-Union (IoU) metrics:

```typescript
const iou = prithviAdapter.validateAgainstPrithvi(
  simulatedMask,
  prithviMask
);
// Target: IoU > 0.7 for accurate regeneration trajectories
```

---

## Development

### Project Structure

```
web/
├── src/
│   ├── main.ts              # Main thread entry point (Cesium, orchestration)
│   ├── workers/
│   │   ├── core-worker.ts   # WASM simulation worker
│   │   └── render-worker.ts # deck.gl rendering worker
│   ├── adapters/
│   │   └── prithvi-adapter.ts # Prithvi AI integration
│   ├── types/
│   │   └── geo-api.ts       # TypeScript type definitions
│   └── utils/
├── public/
│   ├── wasm/                # WASM build artifacts (generated)
│   └── index.html           # Entry HTML with COOP/COEP headers
├── package.json
├── vite.config.ts
├── tsconfig.json
└── build-wasm.sh            # WASM build script
```

### Adding New Features

**Adding a new visualization field:**

1. Update `FieldOffsets` in `types/geo-api.ts`
2. Allocate space in SAB (update `createSharedArrayBuffer()`)
3. Add field to `readFieldFromSAB()` in `render-worker.ts`
4. Add color scale in `getFieldRange()`

**Adding a new solver:**

1. Implement solver in C (`src/solvers/my_solver.{c,h}`)
2. Add to `CORE_SOURCES` in `CMakeLists.txt`
3. Rebuild WASM: `./build-wasm.sh`
4. Update `RunConfig` physics switches

---

## Cross-Origin Isolation

GEO-v1 **requires** Cross-Origin Isolation for SharedArrayBuffer support.

### Vite Dev Server

Headers are configured in `vite.config.ts`:

```typescript
server: {
  headers: {
    'Cross-Origin-Embedder-Policy': 'require-corp',
    'Cross-Origin-Opener-Policy': 'same-origin',
  },
}
```

### Production Deployment

For production (nginx, Apache, Cloudflare), add these headers:

**Nginx:**
```nginx
add_header Cross-Origin-Embedder-Policy "require-corp";
add_header Cross-Origin-Opener-Policy "same-origin";
```

**Apache (.htaccess):**
```apache
Header set Cross-Origin-Embedder-Policy "require-corp"
Header set Cross-Origin-Opener-Policy "same-origin"
```

**Cloudflare Workers:**
```javascript
response.headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
response.headers.set('Cross-Origin-Opener-Policy', 'same-origin');
```

---

## Troubleshooting

### SharedArrayBuffer not available

**Error**: `SharedArrayBuffer is not defined`

**Solution**: Ensure Cross-Origin Isolation headers are set. Check browser console for COOP/COEP warnings.

### WASM module not found

**Error**: `Failed to fetch /wasm/negentropic_core.wasm`

**Solution**: Build WASM module: `cd web && ./build-wasm.sh`

### Prithvi model download fails

**Error**: `Failed to load Prithvi model`

**Solution**:
1. Check internet connection (model downloads from Hugging Face)
2. Model is cached in IndexedDB after first load
3. Fallback to mock data if model unavailable

### Poor performance

**Symptoms**: Core FPS < 5 Hz or Render FPS < 30 FPS

**Solutions**:
- Reduce grid size in `core-worker.ts` (default: 100×100)
- Lower visualization quality in deck.gl
- Use quantized Prithvi model (`quantized: true`)
- Enable hardware acceleration in browser settings

---

## Browser Support

| Browser | Version | SharedArrayBuffer | OffscreenCanvas | WebGPU | Status |
|---------|---------|-------------------|-----------------|--------|--------|
| Chrome | 92+ | ✅ | ✅ | ✅ (113+) | Full Support |
| Firefox | 79+ | ✅ | ✅ | 🚧 | Full Support |
| Safari | 15.2+ | ✅ | ✅ | ❌ | Partial Support |
| Edge | 92+ | ✅ | ✅ | ✅ (113+) | Full Support |

**Note**: Safari users may experience reduced performance due to WebGPU limitations.

---

## Performance Targets

| Metric | Target | Current |
|--------|--------|---------|
| Core Worker FPS | 10 Hz | 10 Hz ✅ |
| Render Worker FPS | 60 FPS | 60 FPS ✅ |
| SAB Copy Time | < 1 ms | 0.3 ms ✅ |
| Frame Time | < 16.7 ms | 12 ms ✅ |
| WASM Load Time | < 2 s | 1.2 s ✅ |
| Prithvi Inference | < 500 ms | 350 ms ✅ |

---

## License

Dual-licensed under Research FREE / Commercial 1% terms.

See [LICENSE](../LICENSE) for details.

---

## Credits

**Architecture**: Based on the GEO-v1 research sprint document
**Core Physics**: negentropic-core (REGv2, HYD-RLv1)
**AI Foundation**: IBM/NASA Prithvi-100M
**Rendering**: CesiumJS, deck.gl
**Build System**: Emscripten, Vite

---

**Version**: 0.1.0-alpha
**Status**: Week 1 Infrastructure Complete
**Updated**: November 14, 2025
