/**
 * Render Worker (Thread 3)
 * Runs high-performance data processing for Cesium Primitive visualization.
 *
 * ============================================================================
 * ORACLE-005 ARCHITECTURAL NOTICE
 * ============================================================================
 *
 * **STATUS:** DECK.GL PURGED (2025-11-19)
 *
 * **REASON:** The deck.gl custom view architecture caused irrecoverable WebGL
 * context poisoning (e.g., "Pixel project matrix not invertible" and
 * "GL_INVALID_OPERATION" errors), preventing Cesium Primitives from rendering.
 *
 * **CURRENT ARCHITECTURE (NUCLEAR OPTION):**
 * - This worker is now a dedicated **Data Processing Worker**.
 * - It reads the SharedArrayBuffer (SAB) at 60 FPS.
 * - It calculates the **Difference Map (SOM change)** colors.
 * - It **posts the final color array** (Uint8Array RGBA) to the main thread.
 * - **Cesium Primitives** in the main thread (main.ts) render the colors.
 *
 * **FUTURE:** This worker is stable and will remain as the core data processing
 * engine for all future geo-visualization layers.
 *
 * ============================================================================
 */

// ============================================================================
// Comprehensive Polyfills (Minimal set retained)
// ============================================================================
// NOTE: All deck.gl-specific mocks (IntersectionObserver, ResizeObserver,
// document/window) are now unnecessary but retained for future library compatibility.

// 1. Global namespace - needed by some modules
// @ts-ignore
if (typeof global === 'undefined') {
  // @ts-ignore
  self.global = self;
}

// 2. HTMLCanvasElement - retained for environment stability
// @ts-ignore
if (typeof HTMLCanvasElement === 'undefined') {
  // @ts-ignore
  self.HTMLCanvasElement = class HTMLCanvasElement extends OffscreenCanvas {
    constructor(width: number, height: number) {
      super(width, height);
      // @ts-ignore
      this.style = {};
      // @ts-ignore
      this.offsetWidth = width;
      // @ts-ignore
      this.offsetHeight = height;
      // @ts-ignore
      this.clientWidth = width;
      // @ts-ignore
      this.clientHeight = height;
    }

    getBoundingClientRect() {
      return {
        // @ts-ignore
        left: 0,
        top: 0,
        // @ts-ignore
        right: this.width,
        // @ts-ignore
        bottom: this.height,
        // @ts-ignore
        width: this.width,
        // @ts-ignore
        height: this.height,
        x: 0,
        y: 0,
      };
    }

    addEventListener() {}
    removeEventListener() {}
    dispatchEvent() { return true; }

    setAttribute() {}
    getAttribute() { return null; }
    removeAttribute() {}
  };
}

// 3. Add missing methods directly to OffscreenCanvas prototype
if (typeof OffscreenCanvas !== 'undefined') {
  // @ts-ignore
  if (!OffscreenCanvas.prototype.getBoundingClientRect) {
    // @ts-ignore
    OffscreenCanvas.prototype.getBoundingClientRect = function() {
      return { left: 0, top: 0, right: this.width, bottom: this.height, width: this.width, height: this.height, x: 0, y: 0 };
    };
  }
  // ... (Other OffscreenCanvas polyfills for style, dimensions, etc. remain unchanged) ...

  // @ts-ignore
  if (!OffscreenCanvas.prototype.addEventListener) {
    // @ts-ignore
    OffscreenCanvas.prototype.addEventListener = function() {};
    // @ts-ignore
    OffscreenCanvas.prototype.removeEventListener = function() {};
    // @ts-ignore
    OffscreenCanvas.prototype.dispatchEvent = function() { return true; };
  }

  // @ts-ignore - Add DOM query methods for legacy compatibility
  if (!OffscreenCanvas.prototype.querySelector) {
    // @ts-ignore
    OffscreenCanvas.prototype.querySelector = function() { return null; };
    // @ts-ignore
    OffscreenCanvas.prototype.querySelectorAll = function() { return []; };
    // @ts-ignore
    OffscreenCanvas.prototype.getElementById = function() { return null; };
    // @ts-ignore
    OffscreenCanvas.prototype.getElementsByClassName = function() { return []; };
    // @ts-ignore
    OffscreenCanvas.prototype.getElementsByTagName = function() { return []; };
    // @ts-ignore
    OffscreenCanvas.prototype.appendChild = function() {};
    // @ts-ignore
    OffscreenCanvas.prototype.removeChild = function() {};
    // @ts-ignore
    OffscreenCanvas.prototype.insertBefore = function() {};
    // @ts-ignore
    OffscreenCanvas.prototype.append = function() {};  // Modern DOM API
    // @ts-ignore
    OffscreenCanvas.prototype.prepend = function() {};  // Modern DOM API
  }

  // @ts-ignore
  if (!OffscreenCanvas.prototype.style) {
    // @ts-ignore - Define style as a getter that returns a style object
    Object.defineProperty(OffscreenCanvas.prototype, 'style', {
      get: function() {
        // @ts-ignore
        if (!this._styleProxy) {
          // @ts-ignore
          this._styleProxy = new Proxy({}, {
            set: (target: any, prop: string, value: any) => {
              target[prop] = value;
              return true;
            },
            get: (target: any, prop: string) => {
              return target[prop];
            }
          });
        }
        // @ts-ignore
        return this._styleProxy;
      }
    });
  }

  // @ts-ignore - Add offset and client dimensions
  Object.defineProperty(OffscreenCanvas.prototype, 'offsetWidth', {
    // @ts-ignore
    get: function() { return this.width; }
  });
  // @ts-ignore
  Object.defineProperty(OffscreenCanvas.prototype, 'offsetHeight', {
    // @ts-ignore
    get: function() { return this.height; }
  });
  // @ts-ignore
  Object.defineProperty(OffscreenCanvas.prototype, 'clientWidth', {
    // @ts-ignore
    get: function() { return this.width; }
  });
  // @ts-ignore
  Object.defineProperty(OffscreenCanvas.prototype, 'clientHeight', {
    // @ts-ignore
    get: function() { return this.height; }
  });
}

// 4. IntersectionObserver - retained for environment stability
// @ts-ignore
if (typeof IntersectionObserver === 'undefined') {
  // @ts-ignore
  self.IntersectionObserver = class IntersectionObserver {
    constructor() {}
    observe() {}
    unobserve() {}
    disconnect() {}
  };
}

// 5. ResizeObserver - retained for environment stability
// @ts-ignore
if (typeof ResizeObserver === 'undefined') {
  // @ts-ignore
  self.ResizeObserver = class ResizeObserver {
    constructor(callback: any) {
      // @ts-ignore
      this.callback = callback;
      // @ts-ignore
      this.observers = [];
    }
    observe(target: any) {
      // @ts-ignore
      this.observers.push(target);
    }
    unobserve(target: any) {
      // @ts-ignore
      this.observers = this.observers.filter((obs: any) => obs !== target);
    }
    disconnect() {
      // @ts-ignore
      this.observers = [];
    }
  };
}

// 6. Window object - retained for environment stability
// @ts-ignore
if (typeof window === 'undefined') {
  // @ts-ignore
  self.window = {
    devicePixelRatio: 1,
    innerWidth: 800,
    innerHeight: 600,
    addEventListener: () => {},
    removeEventListener: () => {},
    matchMedia: () => ({
      matches: false,
      media: '',
      addListener: () => {},
      removeListener: () => {},
      addEventListener: () => {},
      removeEventListener: () => {},
      dispatchEvent: () => true,
    }),
    navigator: {
      userAgent: 'Worker',
      platform: 'Worker',
    },
  };
}

// 7. Document object - retained for environment stability
// @ts-ignore
if (typeof document === 'undefined') {
  // @ts-ignore
  self.document = {
    createElement: (tag: string) => {
      // Create a fake element with all the properties deck.gl might need
      const fakeElement: any = {
        style: new Proxy({}, {
          set: (target: any, prop: string, value: any) => {
            target[prop] = value;
            return true;
          },
          get: (target: any, prop: string) => {
            return target[prop];
          }
        }),
        addEventListener: () => {},
        removeEventListener: () => {},
        dispatchEvent: () => true,
        appendChild: () => {},
        removeChild: () => {},
        insertBefore: () => {},
        append: () => {},  // Modern DOM API for appending nodes
        prepend: () => {},  // Modern DOM API for prepending nodes
        setAttribute: () => {},
        getAttribute: () => null,
        removeAttribute: () => {},
        getBoundingClientRect: () => ({
          left: 0, top: 0, right: 0, bottom: 0,
          width: 0, height: 0, x: 0, y: 0
        }),
        // DOM query methods - critical for WidgetManager
        querySelector: () => null,
        querySelectorAll: () => [],
        getElementById: () => null,
        getElementsByClassName: () => [],
        getElementsByTagName: () => [],
        tagName: tag.toUpperCase(),
        nodeName: tag.toUpperCase(),
        nodeType: 1,
        offsetWidth: 0,
        offsetHeight: 0,
        clientWidth: 0,
        clientHeight: 0,
      };

      // For canvas elements, add getContext
      if (tag === 'canvas') {
        fakeElement.getContext = () => null;
        fakeElement.width = 0;
        fakeElement.height = 0;
      }

      return fakeElement;
    },
    createElementNS: (ns: string, tag: string) => {
      // @ts-ignore
      return self.document.createElement(tag);
    },
    body: {
      appendChild: () => {},
      removeChild: () => {},
      style: {},
    },
    documentElement: {
      style: {},
    },
    addEventListener: () => {},
    removeEventListener: () => {},
    dispatchEvent: () => true,
  };
}

import type {
  RenderWorkerMessage,
  FieldOffsets,
  SABHeader,
} from '../types/geo-api';

// ============================================================================
// Deck.gl PURGE (Removed All Dynamic Imports and Global State)
// ============================================================================

// let Deck: any; // PURGED
// let GridLayer: any; // PURGED
// ... All other deck.gl imports and globals PURGED

// ============================================================================
// Worker State (Simplified)
// ============================================================================

let sab: SharedArrayBuffer | null = null;
let fieldOffsets: FieldOffsets | null = null;

let isRunning: boolean = false;
let frameCount: number = 0;
let lastFPSUpdate: number = Date.now();

// SAB configuration
const SAB_HEADER_SIZE = 128;
const SAB_SIGNAL_INDEX = 0;

// Grid configuration (Keep for data processing)
let gridRows: number = 100;
let gridCols: number = 100;

// Visualization state (Keep only what's needed for difference map logic)
let currentField: 'theta' | 'som' | 'vegetation' | 'temperature' = 'theta';
let colorScale: [number, number] = [0, 1];

// Layer visibility controls (Simplified for the difference layer)
let showDifferenceMap: boolean = true;

// Baseline for difference map (captured at simulation start)
let somBaseline: Float32Array | null = null;

// ============================================================================
// DECK.GL PURGE: Custom MatrixView Architecture
// ============================================================================

// The WeakMap and the entire createMatrixViewClass function have been PURGED.

// ============================================================================
// DECK.GL PURGE: Module Loading and Initialization
// ============================================================================

/**
 * @PURGED - deck.gl module loading is no longer required.
 * async function loadDeckModules() { ... }
 */

/**
 * @PURGED - deck.gl initialization and WebGL context setup is no longer required.
 * function initializeDeck(canvas: OffscreenCanvas) { ... }
 */

// ============================================================================
// Color Utility (Replaces Deck.gl Color Aggregation)
// ============================================================================

/**
 * CliMA RdBu-11 Colormap (Red=Degradation, Blue=Restoration)
 * Values map to normalizedDelta: [-1.0, -0.8, -0.6, -0.4, -0.2, 0.0, 0.2, 0.4, 0.6, 0.8, 1.0]
 * The last element is the alpha value.
 */
const COLOR_STOPS: [number, number, number, number][] = [
  [165, 0, 38, 200],      // -1.0 (Dark red)
  [215, 48, 39, 195],     // -0.8
  [244, 109, 67, 190],    // -0.6
  [253, 174, 97, 185],    // -0.4
  [254, 224, 144, 180],   // -0.2
  [255, 255, 191, 0],     // 0.0 (White, transparent) - CRITICAL: Alpha is 0 for zero-change
  [224, 243, 248, 180],   // 0.2
  [171, 217, 233, 185],   // 0.4
  [116, 173, 209, 190],   // 0.6
  [69, 117, 180, 195],    // 0.8
  [49, 54, 149, 200],     // 1.0 (Dark blue)
];

/**
 * Performs linear interpolation between two color stops.
 * @param color1 - [r, g, b, a] for the lower stop
 * @param color2 - [r, g, b, a] for the upper stop
 * @param factor - Interpolation factor (0 to 1)
 * @returns An array of [r, g, b, a]
 */
function interpolateColor(color1: number[], color2: number[], factor: number): number[] {
  const result = [];
  for (let i = 0; i < 4; i++) {
    result[i] = Math.round(color1[i] + (color2[i] - color1[i]) * factor);
  }
  return result;
}

/**
 * Maps a normalized delta value [-1.0, 1.0] to an RGBA color array [r, g, b, a].
 * @param normalizedDelta - A value between -1.0 and 1.0.
 * @returns A 4-element array [r, g, b, a] as Uint8.
 */
function mapDeltaToColor(normalizedDelta: number): number[] {
  // Clamp delta to the range [-1.0, 1.0]
  const clampedDelta = Math.max(-1.0, Math.min(1.0, normalizedDelta));

  // The 11 color stops are spaced every 0.2 delta: -1.0, -0.8, ..., 0.8, 1.0
  // Step size is 0.2
  // Total indices are 0 to 10
  const step = 0.2;
  const index = (clampedDelta + 1.0) / step; // Maps [-1, 1] to [0, 10]

  // If index is exactly an integer, use that stop
  if (index === Math.round(index)) {
    return COLOR_STOPS[Math.round(index)];
  }

  // Find the two surrounding stops for interpolation
  const lowerIndex = Math.floor(index);
  const upperIndex = Math.ceil(index);

  const lowerColor = COLOR_STOPS[lowerIndex];
  const upperColor = COLOR_STOPS[upperIndex];

  // Calculate the interpolation factor (0 to 1)
  const lowerDeltaValue = lowerIndex * step - 1.0;
  const interpolationFactor = (clampedDelta - lowerDeltaValue) / step;

  return interpolateColor(lowerColor, upperColor, interpolationFactor);
}


// ============================================================================
// Cesium Primitive Data Generation (Replaces Deck.gl Layer Logic)
// ============================================================================

/**
 * New core logic: Calculates the difference map colors and posts them to the main thread.
 * This function replaces the complexity of the deck.gl updateLayers.
 */
function calculateAndPostColorsForCesium() {
  if (!sab || !somBaseline) {
    // Post empty data if no baseline is set, or post the mean SOM for debugging
    if (frameCount === 0) {
      const header = readHeader();
      if (header) {
        console.log(`[Render] Waiting for SOM baseline. Current grid: ${header.rows}x${header.cols}`);
      } else {
        console.log('[Render] Waiting for SAB and baseline data...');
      }
    }
    return;
  }

  const header = readHeader();
  const currentData = readFieldFromSAB('som');

  if (!header || !currentData) {
    console.warn('[Render] Data flow error: SAB header or current data is missing.');
    return;
  }

  // --- 1. Compute Deltas and Normalized Deltas (copied from convertDifferenceToGrid) ---

  // Normalization factor: ±30% SOM change is considered max range for regenerative agriculture
  const MAX_EXPECTED_DELTA = 0.3;
  const numCells = gridRows * gridCols;
  const colorArray = new Uint8Array(numCells * 4); // RGBA (4 bytes per cell)

  // Track stats for logging
  let sumDelta = 0;
  let minDelta = Infinity;
  let maxDelta = -Infinity;

  for (let i = 0; i < numCells; i++) {
    const current = currentData[i];
    const baseline = somBaseline[i];
    const delta = current - baseline;

    sumDelta += delta;
    minDelta = Math.min(minDelta, delta);
    maxDelta = Math.max(maxDelta, delta);

    // Normalize delta to [-1, +1] range
    const normalizedDelta = Math.max(-1, Math.min(1, delta / MAX_EXPECTED_DELTA));

    // --- 2. Map Delta to Color ---
    const [r, g, b, a] = mapDeltaToColor(normalizedDelta);

    // Write RGBA into the color array
    const offset = i * 4;
    colorArray[offset] = r;
    colorArray[offset + 1] = g;
    colorArray[offset + 2] = b;
    colorArray[offset + 3] = a;
  }

  // --- 3. Post Message to Main Thread (Cesium Primitive Update) ---

  // Log delta statistics every 100 frames for monitoring
  if (frameCount === 0 || frameCount % 100 === 0) {
    const meanDelta = sumDelta / numCells;
    console.log('[Render] Data Processing Complete. Posting new colors.');
    console.log('[Render] Delta statistics (for main thread log confirmation):', {
      min: minDelta.toFixed(5),
      max: maxDelta.toFixed(5),
      mean: meanDelta.toFixed(5),
    });
  }

  // Transfer the Uint8Array back to the main thread for zero-copy performance
  postMessage({
    type: 'new-colors',
    payload: {
      colors: colorArray.buffer, // Transfer the underlying ArrayBuffer
      rows: header.rows,
      cols: header.cols,
      bbox: header.bbox,
      timestamp: header.timestamp,
    }
  }, [colorArray.buffer]); // CRITICAL: Transferable object

}

// ============================================================================
// Render Loop (Simplified)
// ============================================================================

function renderLoop() {
  if (!isRunning || !sab) {
    return;
  }

  try {
    // Wait for signal from Core Worker (non-blocking) - KEEP for timing sync
    const int32View = new Int32Array(sab);
    // Note: We don't block, we just read the latest state
    Atomics.load(int32View, SAB_SIGNAL_INDEX);

    // The entire deck.gl viewState update and redraw is PURGED.
    // Replaced by data calculation and post message.
    calculateAndPostColorsForCesium();

    // Update FPS counter
    frameCount++;
    const now = Date.now();
    if (now - lastFPSUpdate >= 1000) {
      const fps = (frameCount * 1000) / (now - lastFPSUpdate);
      postMessage({ type: 'metrics', payload: { fps: fps.toFixed(1) } });
      frameCount = 0;
      lastFPSUpdate = now;
    }

  } catch (error) {
    console.error('Render loop error:', error);
    isRunning = false;
    postMessage({ type: 'error', payload: { message: String(error) } });
    return;
  }

  // Schedule next frame (60 FPS)
  requestAnimationFrame(renderLoop);
}

// ============================================================================
// SharedArrayBuffer Operations (Keep)
// ============================================================================

// ... (readHeader and readFieldFromSAB functions remain unchanged) ...

function readHeader(): SABHeader | null {
  if (!sab) return null;

  const headerView = new DataView(sab);

  let offset = 4; // Skip first 4 bytes (Atomics signal)

  // Read in SAME ORDER as core-worker writes
  const version = headerView.getUint32(offset, true);
  offset += 4;

  const epoch = headerView.getUint32(offset, true);
  offset += 4;

  const rows = headerView.getUint32(offset, true);
  offset += 4;

  const cols = headerView.getUint32(offset, true);
  offset += 4;

  // Timestamp (8 bytes) comes BEFORE bbox
  const timestamp = headerView.getFloat64(offset, true);
  offset += 8;

  // BBox (4 floats = 16 bytes) comes AFTER timestamp
  const bbox = {
    minLon: headerView.getFloat32(offset, true),
    minLat: headerView.getFloat32(offset + 4, true),
    maxLon: headerView.getFloat32(offset + 8, true),
    maxLat: headerView.getFloat32(offset + 12, true),
  };

  const header: SABHeader = {
    version,
    epoch,
    gridID: 'default',
    bbox,
    stride: cols,
    rows,
    cols,
    timestamp,
  };

  return header;
}

function readFieldFromSAB(fieldName: keyof FieldOffsets): Float32Array | null {
  if (!sab || !fieldOffsets) return null;

  const offset = fieldOffsets[fieldName];
  if (offset === undefined) return null;

  const fieldSize = gridRows * gridCols;

  // Create a Float32Array view of the field data
  const float32View = new Float32Array(
    sab,
    offset,
    fieldSize
  );

  // Copy to a new array (to avoid holding reference to SAB)
  return new Float32Array(float32View);
}

// ============================================================================
// DECK.GL PURGE: Helper Functions
// ============================================================================

// All convertFieldToGrid, convertDifferenceToGrid, getCellSize PURGED or integrated.

// ============================================================================
// DECK.GL PURGE: Canvas Resize Handler
// ============================================================================

/**
 * @PURGED - Canvas resizing is handled by the main thread Cesium viewport.
 * function handleResize(width: number, height: number) { ... }
 */

// ============================================================================
// Message Handlers (Updated for Purge)
// ============================================================================

self.onmessage = async (e: MessageEvent<RenderWorkerMessage>) => {
  const { type, payload } = e.data;

  try {
    switch (type) {
      case 'init':
        if (payload?.sab) { // offscreenCanvas is no longer needed
          sab = payload.sab;
          fieldOffsets = payload.fieldOffsets || null;

          console.log('Render Worker received SAB. Deck.gl purged. Starting data worker pipeline.');

          // The canvas init, module load, and deck.gl init are PURGED.
          // We immediately signal ready, as no complex GL setup is required.
          postMessage({ type: 'ready' });
        }
        break;

      case 'start':
        if (!isRunning) {
          isRunning = true;
          frameCount = 0;
          lastFPSUpdate = Date.now();
          console.log('Render Worker: Starting data processing loop');
          renderLoop();
        }
        break;

      case 'pause':
        isRunning = false;
        console.log('Render Worker: Paused');
        break;

      case 'config':
        // Update visualization config (simplified)
        if (payload) {
          if (payload.field) {
            currentField = payload.field;
          }
          if (payload.colorScale) {
            colorScale = payload.colorScale;
          }
          // Layer visibility toggles (simplified to only difference map)
          if (payload.showDifferenceMap !== undefined) {
            showDifferenceMap = payload.showDifferenceMap;
          }
          // Capture baseline command
          if (payload.captureBaseline) {
            const somData = readFieldFromSAB('som');
            if (somData) {
              somBaseline = new Float32Array(somData);
              const cells = somData.length;
              const meanSOM = somData.reduce((a, b) => a + b, 0) / cells;

              console.log(`[DifferenceMap] ✓ Baseline captured: ${cells} cells, mean SOM = ${meanSOM.toFixed(4)}`);

              postMessage({
                type: 'baseline-captured',
                payload: {
                  cells,
                  meanSOM: meanSOM,
                }
              });
            } else {
              console.warn('[Render] ✗ Cannot capture baseline - no SOM data available');
              postMessage({
                type: 'baseline-error',
                payload: { message: 'No SOM data available' }
              });
            }
          }
        }
        break;

      case 'camera-sync':
        // @PURGED: No longer needed. Cesium handles the view and we send the raw matrices to it.
        // The worker only calculates data/colors.
        break;

      case 'resize':
        // @PURGED: No longer needed.
        break;

      default:
        console.warn('Unknown message type:', type);
    }

  } catch (error) {
    console.error('Render Worker error:', error);
    postMessage({
      type: 'error',
      payload: { message: String(error) },
    });
  }
};

// Signal that worker script is loaded
try {
  postMessage({ type: 'worker-loaded' });
  console.log('Render Worker script loaded, awaiting init...');
} catch (error) {
  console.error('Render Worker failed to initialize:', error);
  postMessage({
    type: 'error',
    payload: { message: `Initialization failed: ${String(error)}` }
  });
}