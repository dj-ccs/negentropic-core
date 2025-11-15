/**
 * Render Worker (Thread 3)
 * Runs deck.gl on OffscreenCanvas, reads SAB at 60 FPS
 */

// ============================================================================
// Comprehensive Polyfills for deck.gl in Worker Context
// ============================================================================
// deck.gl is designed for main thread with DOM APIs. These polyfills make it
// compatible with OffscreenCanvas in a worker environment.

// 1. Global namespace - deck.gl expects 'global' to be defined
// @ts-ignore
if (typeof global === 'undefined') {
  // @ts-ignore
  self.global = self;
}

// 2. HTMLCanvasElement - deck.gl checks for HTMLCanvasElement
// We'll create a wrapper class that extends OffscreenCanvas with DOM-like methods
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

    // getBoundingClientRect is required by deck.gl's canvas context
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

    // Event listener stubs
    addEventListener() {}
    removeEventListener() {}
    dispatchEvent() { return true; }

    // Style property stubs
    setAttribute() {}
    getAttribute() { return null; }
    removeAttribute() {}
  };
}

// 3. Add missing methods directly to OffscreenCanvas prototype
// This ensures existing OffscreenCanvas instances also have these methods
if (typeof OffscreenCanvas !== 'undefined') {
  // @ts-ignore
  if (!OffscreenCanvas.prototype.getBoundingClientRect) {
    // @ts-ignore
    OffscreenCanvas.prototype.getBoundingClientRect = function() {
      return {
        left: 0,
        top: 0,
        right: this.width,
        bottom: this.height,
        width: this.width,
        height: this.height,
        x: 0,
        y: 0,
      };
    };
  }

  // @ts-ignore
  if (!OffscreenCanvas.prototype.addEventListener) {
    // @ts-ignore
    OffscreenCanvas.prototype.addEventListener = function() {};
    // @ts-ignore
    OffscreenCanvas.prototype.removeEventListener = function() {};
    // @ts-ignore
    OffscreenCanvas.prototype.dispatchEvent = function() { return true; };
  }

  // @ts-ignore - Add DOM query methods for deck.gl WidgetManager
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

// 4. IntersectionObserver - deck.gl uses this for viewport culling
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

// 5. ResizeObserver - deck.gl uses this for canvas resizing
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

// 6. Window object - deck.gl accesses window for devicePixelRatio and dimensions
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

// 7. Document object - deck.gl tries to create elements for feature detection
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

// Deck.gl modules - loaded dynamically
let Deck: any;
let ScatterplotLayer: any;
let deckModulesLoaded = false;

// ============================================================================
// Worker State
// ============================================================================

let deck: any = null; // Deck instance
let offscreenCanvas: OffscreenCanvas | null = null;
let sab: SharedArrayBuffer | null = null;
let fieldOffsets: FieldOffsets | null = null;
let isRunning: boolean = false;
let frameCount: number = 0;
let lastFPSUpdate: number = Date.now();

// SAB configuration
const SAB_HEADER_SIZE = 128;
const SAB_SIGNAL_INDEX = 0;

// Grid configuration
let gridRows: number = 100;
let gridCols: number = 100;

// Visualization state
let currentField: 'theta' | 'som' | 'vegetation' | 'temperature' = 'theta';
let colorScale: [number, number] = [0, 1];

// ============================================================================
// Deck.gl Module Loading
// ============================================================================

async function loadDeckModules() {
  if (deckModulesLoaded) return;

  try {
    console.log('Loading deck.gl modules...');
    const deckCore = await import('@deck.gl/core');
    const deckLayers = await import('@deck.gl/layers');
    Deck = deckCore.Deck;
    ScatterplotLayer = deckLayers.ScatterplotLayer;
    deckModulesLoaded = true;
    console.log('✓ Deck.gl modules loaded in Render Worker');
  } catch (error) {
    console.error('Failed to load deck.gl modules:', error);
    throw new Error(`Deck.gl loading failed: ${String(error)}`);
  }
}

// ============================================================================
// Deck.gl Initialization
// ============================================================================

function initializeDeck(canvas: OffscreenCanvas) {
  if (!deckModulesLoaded) {
    throw new Error('Deck.gl modules not loaded');
  }
  try {
    // Ensure canvas has all necessary properties for deck.gl
    // @ts-ignore - Add style proxy if not already present
    if (!canvas.style || typeof canvas.style !== 'object') {
      // @ts-ignore
      canvas.style = new Proxy({}, {
        set: (target: any, prop: string, value: any) => {
          target[prop] = value;
          return true;
        },
        get: (target: any, prop: string) => {
          return target[prop];
        }
      });
    }

    // Ensure getBoundingClientRect exists
    // @ts-ignore
    if (!canvas.getBoundingClientRect) {
      // @ts-ignore
      canvas.getBoundingClientRect = function() {
        return {
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
      };
    }

    // Add event listener stubs
    // @ts-ignore
    if (!canvas.addEventListener) {
      // @ts-ignore
      canvas.addEventListener = function() {};
      // @ts-ignore
      canvas.removeEventListener = function() {};
      // @ts-ignore
      canvas.dispatchEvent = function() { return true; };
    }

    // Add querySelector and DOM query methods for WidgetManager
    // @ts-ignore
    if (!canvas.querySelector) {
      // @ts-ignore
      canvas.querySelector = function() { return null; };
      // @ts-ignore
      canvas.querySelectorAll = function() { return []; };
      // @ts-ignore
      canvas.getElementById = function() { return null; };
      // @ts-ignore
      canvas.getElementsByClassName = function() { return []; };
      // @ts-ignore
      canvas.getElementsByTagName = function() { return []; };
    }

    // Add appendChild/removeChild for widget container
    // @ts-ignore
    if (!canvas.appendChild) {
      // @ts-ignore
      canvas.appendChild = function() {};
      // @ts-ignore
      canvas.removeChild = function() {};
      // @ts-ignore
      canvas.insertBefore = function() {};
      // @ts-ignore
      canvas.append = function() {};  // Modern DOM API
      // @ts-ignore
      canvas.prepend = function() {};  // Modern DOM API
    }

    // Create a minimal WebGL context for deck.gl
    const gl = canvas.getContext('webgl2', {
      alpha: true,
      antialias: true,
      premultipliedAlpha: false,
    });

    if (!gl) {
      throw new Error('Failed to get WebGL2 context');
    }

    // Wrap WebGL context to ensure canvas reference is accessible
    // @ts-ignore
    if (!gl.canvas) {
      // @ts-ignore
      gl.canvas = canvas;
    }

    // Initialize deck.gl with OffscreenCanvas
    deck = new Deck({
      canvas: canvas as any,
      gl,
      width: canvas.width,
      height: canvas.height,
      initialViewState: {
        longitude: 0,
        latitude: 0,
        zoom: 3,
        pitch: 0,
        bearing: 0,
      },
      controller: false, // Disable controller in worker (main thread handles camera)
      layers: [],
      // Disable all UI widgets - they're DOM-based and don't work in workers
      _typedArrayManagerProps: null,
      useDevicePixels: false, // Disable DPR scaling to avoid widget issues
    });

    console.log('✓ Deck.gl initialized in Render Worker');

  } catch (error) {
    console.error('deck: Failed to initialize deck.gl:', error);
    throw error;
  }
}

// ============================================================================
// Render Loop
// ============================================================================

function renderLoop() {
  if (!isRunning || !sab || !deck) {
    return;
  }

  try {
    // Wait for signal from Core Worker (non-blocking)
    const int32View = new Int32Array(sab);
    const currentSignal = Atomics.load(int32View, SAB_SIGNAL_INDEX);

    // Read field data from SAB
    const fieldData = readFieldFromSAB(currentField);

    if (fieldData) {
      // Update deck.gl layers
      updateLayers(fieldData);
    } else if (frameCount === 0) {
      console.log('[Render] No field data available yet');
    }

    // Render frame
    deck.redraw();

    // Update FPS counter
    frameCount++;
    const now = Date.now();
    if (now - lastFPSUpdate >= 1000) {
      const fps = (frameCount * 1000) / (now - lastFPSUpdate);
      postMessage({ type: 'metrics', payload: { fps } });
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
// SharedArrayBuffer Operations
// ============================================================================

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
// Deck.gl Layer Updates
// ============================================================================

function updateLayers(fieldData: Float32Array) {
  if (!deck) return;

  const header = readHeader();
  if (!header) {
    console.log('[Render] No header available');
    return;
  }

  // Convert field data to grid points for ScatterplotLayer
  const gridData = convertFieldToGrid(fieldData, header);

  // Debug: Log layer update info (only first time and every 100 frames)
  if (frameCount === 0 || frameCount % 100 === 0) {
    console.log('[Render] Updating layers:', {
      points: gridData.length,
      bbox: header.bbox,
      sampleValues: fieldData.slice(0, 5),
      dataRange: [Math.min(...fieldData), Math.max(...fieldData)],
    });
  }

  // Define color scale based on field type
  const [minVal, maxVal] = getFieldRange(currentField);

  const layers = [
    new ScatterplotLayer({
      id: 'field-grid',
      data: gridData,
      pickable: true,
      stroked: false,
      filled: true,
      radiusScale: 1000,
      radiusMinPixels: 2,
      radiusMaxPixels: 100,
      getPosition: (d: any) => d.position,
      getRadius: (d: any) => 500, // 500 meters radius per point
      getFillColor: (d: any) => valueToColor(d.value, minVal, maxVal),
      opacity: 0.8,
    }),
  ];

  deck.setProps({ layers });
}

function convertFieldToGrid(fieldData: Float32Array, header: SABHeader) {
  const gridData: any[] = [];

  const lonStep = (header.bbox.maxLon - header.bbox.minLon) / gridCols;
  const latStep = (header.bbox.maxLat - header.bbox.minLat) / gridRows;

  for (let row = 0; row < gridRows; row++) {
    for (let col = 0; col < gridCols; col++) {
      const idx = row * gridCols + col;
      const value = fieldData[idx];

      const lon = header.bbox.minLon + (col + 0.5) * lonStep;
      const lat = header.bbox.minLat + (row + 0.5) * latStep;

      gridData.push({
        position: [lon, lat],
        value: value,
      });
    }
  }

  return gridData;
}

function getCellSize(header: SABHeader): number {
  // Calculate cell size in degrees
  const lonStep = (header.bbox.maxLon - header.bbox.minLon) / gridCols;
  const latStep = (header.bbox.maxLat - header.bbox.minLat) / gridRows;

  return Math.max(lonStep, latStep) * 111320; // Convert to meters (approx)
}

function getFieldRange(fieldName: string): [number, number] {
  switch (fieldName) {
    case 'theta':
      return [0, 0.5]; // soil moisture (0-50%)
    case 'som':
      return [0, 10]; // SOM (0-10%)
    case 'vegetation':
      return [0, 1]; // vegetation cover (0-100%)
    case 'temperature':
      return [0, 40]; // temperature (0-40°C)
    default:
      return [0, 1];
  }
}

function valueToColor(value: number, min: number, max: number): [number, number, number, number] {
  // Normalize value to 0-1
  const normalized = Math.max(0, Math.min(1, (value - min) / (max - min)));

  // Color scale: blue (low) -> green (medium) -> red (high)
  if (normalized < 0.5) {
    const t = normalized * 2;
    return [
      Math.floor(0 * (1 - t) + 0 * t),
      Math.floor(0 * (1 - t) + 255 * t),
      Math.floor(255 * (1 - t) + 0 * t),
      200, // alpha
    ];
  } else {
    const t = (normalized - 0.5) * 2;
    return [
      Math.floor(0 * (1 - t) + 255 * t),
      Math.floor(255 * (1 - t) + 165 * t),
      Math.floor(0 * (1 - t) + 0 * t),
      200, // alpha
    ];
  }
}

// ============================================================================
// Canvas Resize Handler
// ============================================================================

function handleResize(width: number, height: number) {
  if (offscreenCanvas) {
    offscreenCanvas.width = width;
    offscreenCanvas.height = height;

    if (deck) {
      deck.setProps({ width, height });
    }
  }
}

// ============================================================================
// Message Handlers
// ============================================================================

self.onmessage = async (e: MessageEvent<RenderWorkerMessage>) => {
  const { type, payload } = e.data;

  try {
    switch (type) {
      case 'init':
        if (payload?.sab && payload?.offscreenCanvas) {
          sab = payload.sab;
          offscreenCanvas = payload.offscreenCanvas;
          fieldOffsets = payload.fieldOffsets || null;

          console.log('Render Worker received SAB and OffscreenCanvas');

          // Load deck.gl modules first
          await loadDeckModules();

          // Initialize deck.gl
          initializeDeck(offscreenCanvas);

          postMessage({ type: 'ready' });
        }
        break;

      case 'start':
        if (!isRunning) {
          isRunning = true;
          frameCount = 0;
          lastFPSUpdate = Date.now();
          console.log('Render Worker: Starting render loop');
          renderLoop();
        }
        break;

      case 'pause':
        isRunning = false;
        console.log('Render Worker: Paused');
        break;

      case 'config':
        // Update visualization config (field to display, color scale, etc.)
        if (payload) {
          if (payload.field) {
            currentField = payload.field;
          }
          if (payload.colorScale) {
            colorScale = payload.colorScale;
          }
        }
        break;

      case 'resize':
        if (payload?.width && payload?.height) {
          handleResize(payload.width, payload.height);
        }
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

// Signal that worker script is loaded (but deck.gl not yet loaded)
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
