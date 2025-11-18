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
let GridLayer: any;
let ScatterplotLayer: any;
let View: any; // Base View class for custom MatrixView
let Viewport: any; // Viewport class for proper coordinate transformation
let _GlobeView: any; // Note: GlobeView is exported as _GlobeView (experimental API) - DEPRECATED by ORACLE-004
let COORDINATE_SYSTEM: any;
let deckModulesLoaded = false;
let matrixView: any; // Global reference to custom MatrixView instance

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

// Camera synchronization state
// GlobeView uses longitude, latitude, altitude (not zoom/pitch/bearing)
let currentViewState: any = {
  longitude: 0,
  latitude: 0,
  altitude: 1.5, // Initial altitude (matches initialViewState for consistency)
};

// Layer visibility controls
let showMoistureLayer: boolean = true;
let showSOMLayer: boolean = true;
let showVegetationLayer: boolean = true;
let showDifferenceMap: boolean = false;

// Baseline for difference map (captured at simulation start)
let somBaseline: Float32Array | null = null;

// ============================================================================
// ORACLE-004: Custom MatrixView for Direct Matrix Injection
// ============================================================================

/**
 * Custom MatrixView class that replaces _GlobeView
 * Accepts raw view and projection matrices from Cesium and injects them directly
 * into the deck.gl rendering pipeline, bypassing all internal projection calculations.
 *
 * This is the architectural solution to the projection mismatch problem.
 */
class MatrixView {
  viewMatrix: Float32Array;
  projectionMatrix: Float32Array;
  id: string;

  constructor(props: any) {
    // Note: We can't actually extend View here because it's not loaded yet
    // Instead, we'll create an object that duck-types as a View
    this.id = props?.id || 'matrix-view';

    // Initialize matrices to identity to avoid "not invertible" error on first frame
    this.viewMatrix = new Float32Array(16);
    this.viewMatrix[0] = this.viewMatrix[5] = this.viewMatrix[10] = this.viewMatrix[15] = 1;

    this.projectionMatrix = new Float32Array(16);
    this.projectionMatrix[0] = this.projectionMatrix[5] = this.projectionMatrix[10] = this.projectionMatrix[15] = 1;
  }

  /**
   * Override getViewport to use raw matrices
   * This method is called by deck.gl to compute the viewport for rendering
   *
   * CRITICAL FIX: Return a proper Viewport instance, not a plain object.
   * deck.gl layers with COORDINATE_SYSTEM.LNGLAT need a proper Viewport
   * with methods like project(), unproject(), and projectPosition() for
   * geographic coordinate transformation.
   */
  getViewport(options: { width: number; height: number }) {
    const { width, height } = options;

    // Create a proper Viewport instance with our raw matrices
    // This enables deck.gl to properly transform LNGLAT coordinates
    return new Viewport({
      id: this.id,
      x: 0,
      y: 0,
      width,
      height,
      viewMatrix: this.viewMatrix,
      projectionMatrix: this.projectionMatrix,
      // Near/far clipping planes to match Cesium's frustum
      near: 0.1,
      far: 100000000.0,
    });
  }

  /**
   * Stub methods to satisfy deck.gl's View interface
   */
  equals() { return false; }
  makeViewport() { return this.getViewport({ width: 800, height: 600 }); }
  getViewStateId() { return this.id; }
}

// ============================================================================
// Deck.gl Module Loading
// ============================================================================

async function loadDeckModules() {
  if (deckModulesLoaded) return;

  try {
    console.log('Loading deck.gl modules...');
    const deckCore = await import('@deck.gl/core');
    const deckAggregationLayers = await import('@deck.gl/aggregation-layers');
    const deckLayers = await import('@deck.gl/layers');

    Deck = deckCore.Deck;
    View = deckCore.View; // Base View class for custom MatrixView
    Viewport = deckCore.Viewport; // Viewport class for proper coordinate transformation
    _GlobeView = deckCore._GlobeView; // DEPRECATED by ORACLE-004
    COORDINATE_SYSTEM = deckCore.COORDINATE_SYSTEM;
    GridLayer = deckAggregationLayers.GridLayer;
    ScatterplotLayer = deckLayers.ScatterplotLayer;

    deckModulesLoaded = true;
    console.log('✓ Deck.gl modules loaded in Render Worker');
    console.log('[DEBUG] Coordinate system:', COORDINATE_SYSTEM.LNGLAT ? 'LNGLAT (geographic coordinates)' : 'Unknown');
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

    // ORACLE-004: Initialize deck.gl with Custom MatrixView (replaces _GlobeView)
    // Create global MatrixView instance for direct matrix injection
    matrixView = new MatrixView({ id: 'matrix-view' });

    deck = new Deck({
      canvas: canvas as any,
      gl,
      width: canvas.width,
      height: canvas.height,
      views: [matrixView],  // CRITICAL ARCHITECTURAL CHANGE: Use MatrixView for raw matrix injection
      initialViewState: {
        longitude: 0,
        latitude: 0,
        altitude: 1.5,  // Fallback initial state (unused with MatrixView)
      },
      controller: false, // Disable controller in worker (main thread handles camera)
      layers: [],
      // Disable all UI widgets - they're DOM-based and don't work in workers
      useDevicePixels: false, // Disable DPR scaling to avoid widget issues
      // CRITICAL: Use transparent background so Cesium globe is visible underneath
      parameters: {
        blend: true,
        blendFunc: [gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA],
        depthTest: true,
      },
      _typedArrayManagerProps: {
        overAlloc: 1,
        poolSize: 0
      },
    });

    // Set transparent clear color so Cesium globe shows through
    gl.clearColor(0, 0, 0, 0);  // Fully transparent black

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

    // Update viewState from camera sync
    deck.setProps({ viewState: currentViewState });

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

    // Log viewState for debugging (first frame and every 100 frames)
    if (frameCount === 0 || frameCount % 100 === 0) {
      console.log('[Camera] GlobeView ViewState:', {
        lon: currentViewState.longitude.toFixed(2),
        lat: currentViewState.latitude.toFixed(2),
        alt: currentViewState.altitude.toFixed(3),  // Altitude (1 unit = viewport height)
      });
    }

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

  // Read all field data
  const thetaData = readFieldFromSAB('theta');
  const somData = readFieldFromSAB('som');
  const vegetationData = readFieldFromSAB('vegetation');

  if (!thetaData || !somData || !vegetationData) {
    console.log('[Render] Waiting for field data...');
    return;
  }

  // Capture baseline on first frame
  if (frameCount === 0 && !somBaseline) {
    somBaseline = new Float32Array(somData);
    console.log('[Render] Captured SOM baseline for difference map');
  }

  // Convert field data to grid points
  const thetaGrid = convertFieldToGrid(thetaData, header, 'theta');
  const somGrid = convertFieldToGrid(somData, header, 'som');
  const vegetationGrid = convertFieldToGrid(vegetationData, header, 'vegetation');

  // Debug: Log layer update info (only first time and every 100 frames)
  if (frameCount === 0 || frameCount % 100 === 0) {
    console.log('[Render] Updating layers:', {
      points: thetaGrid.length,
      bbox: header.bbox,
      thetaRange: [Math.min(...thetaData), Math.max(...thetaData)],
      somRange: [Math.min(...somData), Math.max(...somData)],
      vegRange: [Math.min(...vegetationData), Math.max(...vegetationData)],
    });
  }

  const layers = [];

  // Layer 1: Moisture Layer (theta)
  // Color: Deep purple (dry) → Vibrant cyan (saturated)
  if (showMoistureLayer) {
    layers.push(
      new GridLayer({
        id: 'moisture-layer',
        data: thetaGrid,
        pickable: true,
        extruded: false,
        cellSize: getCellSize(header),
        coverage: 1,
        getPosition: (d: any) => d.position,
        getColorWeight: (d: any) => d.value,
        getElevationWeight: (d: any) => 0,
        colorAggregation: 'MEAN' as any,
        colorRange: [
          [75, 0, 130, 180],    // Deep purple (dry) - RGB(75,0,130) = Indigo
          [138, 43, 226, 190],  // Blue violet
          [0, 191, 255, 200],   // Deep sky blue
          [0, 255, 255, 210],   // Cyan (wet)
          [64, 224, 208, 220],  // Turquoise (saturated)
        ],
        opacity: 0.7,
      })
    );
  }

  // Layer 2: Soil Organic Matter Layer (SOM)
  // Color: Pale tan (low SOM) → Rich dark brown (high SOM)
  if (showSOMLayer) {
    layers.push(
      new GridLayer({
        id: 'som-layer',
        data: somGrid,
        pickable: true,
        extruded: false,
        cellSize: getCellSize(header),
        coverage: 1,
        getPosition: (d: any) => d.position,
        getColorWeight: (d: any) => d.value,
        getElevationWeight: (d: any) => 0,
        colorAggregation: 'MEAN' as any,
        colorRange: [
          [210, 180, 140, 160],  // Pale tan (low SOM)
          [188, 143, 143, 170],  // Rosy brown
          [160, 82, 45, 180],    // Sienna (medium)
          [101, 67, 33, 190],    // Dark brown
          [54, 36, 18, 200],     // Rich dark brown (high SOM)
        ],
        opacity: 0.6,
      })
    );
  }

  // Layer 3: Vegetation Layer (vegetation_cover)
  // Color: Brown (bare) → Vibrant green (forest)
  // Elevation: Terrain "lifts" based on vegetation density
  if (showVegetationLayer) {
    layers.push(
      new GridLayer({
        id: 'vegetation-layer',
        data: vegetationGrid,
        pickable: true,
        extruded: true,  // Enable 3D elevation
        cellSize: getCellSize(header),
        coverage: 1,
        elevationScale: 5000,  // Scale factor for visual "lift"
        getPosition: (d: any) => d.position,
        getColorWeight: (d: any) => d.value,
        getElevationWeight: (d: any) => d.value,  // Elevation based on vegetation
        colorAggregation: 'MEAN' as any,
        elevationAggregation: 'MEAN' as any,
        colorRange: [
          [101, 67, 33, 180],    // Brown (bare soil)
          [139, 90, 43, 190],    // Saddle brown
          [154, 205, 50, 200],   // Yellow green (grass)
          [34, 139, 34, 210],    // Forest green
          [0, 100, 0, 220],      // Dark green (dense forest)
        ],
        opacity: 0.8,
      })
    );
  }

  // Layer 4: Difference Map (SOM change visualization)
  // Shows restoration impact: green = soil improvement, red = degradation
  if (showDifferenceMap && somBaseline) {
    const differenceGrid = convertDifferenceToGrid(somData, somBaseline, header);

    layers.push(
      new GridLayer({
        id: 'difference-layer',
        data: differenceGrid,
        pickable: true,
        extruded: false,
        cellSize: getCellSize(header),
        coverage: 1,
        getPosition: (d: any) => d.position,
        getColorWeight: (d: any) => d.delta,
        getElevationWeight: (d: any) => 0,
        colorAggregation: 'MEAN' as any,
        // Symmetric color scale: red (loss) → white (no change) → green (gain)
        colorRange: [
          [139, 0, 0, 200],      // Dark red (degradation)
          [255, 69, 0, 190],     // Red-orange
          [255, 255, 255, 0],    // White (no change) - transparent
          [0, 200, 0, 190],      // Green
          [0, 100, 0, 200],      // Dark green (restoration)
        ],
        opacity: 0.9,
      })
    );
  }

  // ============================================================================
  // TEST LAYER: Geographic Coordinate Verification
  // ============================================================================
  // Red circle at Kansas, USA [-95, 40] with 50km radius
  // This layer should MOVE and SCALE with the globe
  // If it stays fixed on screen, geographic coordinates are broken
  layers.push(
    new ScatterplotLayer({
      id: 'test-geo-layer',
      data: [
        {
          position: [-95, 40],  // Kansas, USA (geographic coordinates)
          radius: 50000,         // 50km in meters
          color: [255, 0, 0],   // Red fill
        },
      ],
      coordinateSystem: COORDINATE_SYSTEM.LNGLAT,  // CRITICAL: Geographic coordinates
      getPosition: (d: any) => d.position,
      getRadius: (d: any) => d.radius,
      radiusUnits: 'meters',  // CRITICAL: Meters, not pixels (trusts altitude sync for all scaling)
      getFillColor: (d: any) => d.color,
      getLineColor: [255, 255, 0],  // Yellow outline
      // REMOVED PIXEL CONSTRAINTS: These conflicted with radiusUnits: 'meters'
      // Previously had: radiusMaxPixels: 200, lineWidthMinPixels: 2
      // Removal allows pure geographic scaling based on altitude sync
      opacity: 0.8,
    })
  );

  // Debug log for test layer (first frame only)
  if (frameCount === 0) {
    console.log('[TEST] Red scatter layer added at [-95, 40], radius: 50km (meters)');
    console.log('[TEST] If this circle does NOT move/scale with globe, projection is broken');
  }

  deck.setProps({ layers });
}

function convertFieldToGrid(
  fieldData: Float32Array,
  header: SABHeader,
  fieldType: 'theta' | 'som' | 'vegetation' | 'temperature'
) {
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

function convertDifferenceToGrid(
  currentData: Float32Array,
  baselineData: Float32Array,
  header: SABHeader
) {
  const gridData: any[] = [];

  const lonStep = (header.bbox.maxLon - header.bbox.minLon) / gridCols;
  const latStep = (header.bbox.maxLat - header.bbox.minLat) / gridRows;

  for (let row = 0; row < gridRows; row++) {
    for (let col = 0; col < gridCols; col++) {
      const idx = row * gridCols + col;
      const current = currentData[idx];
      const baseline = baselineData[idx];
      const delta = current - baseline;

      const lon = header.bbox.minLon + (col + 0.5) * lonStep;
      const lat = header.bbox.minLat + (row + 0.5) * latStep;

      gridData.push({
        position: [lon, lat],
        delta: delta,
        current: current,
        baseline: baseline,
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

          // FIX A: Camera Matrix Timing Issue
          // Delay initial layer draw by 100ms to ensure Cesium camera has sent a valid matrix.
          // The "Pixel project matrix not invertible" error occurs when deck.redraw() is called
          // before the camera-sync messages provide a stable viewState.
          setTimeout(() => {
            // CRITICAL FIX: Add test layer immediately after init to fix initial invisibility
            // This ensures the layer is visible BEFORE the simulation starts.
            // The test layer (red dot at Kansas) is added by updateLayers() regardless of fieldData.
            // Pass empty Float32Array since we don't have simulation data yet.
            updateLayers(new Float32Array(0));
            deck.redraw();

            console.log('[INIT] Test layer added and initial frame rendered (delayed 100ms)');

            // Signal ready AFTER the initial draw completes
            postMessage({ type: 'ready' });
          }, 100);
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
          // Layer visibility toggles
          if (payload.showMoistureLayer !== undefined) {
            showMoistureLayer = payload.showMoistureLayer;
          }
          if (payload.showSOMLayer !== undefined) {
            showSOMLayer = payload.showSOMLayer;
          }
          if (payload.showVegetationLayer !== undefined) {
            showVegetationLayer = payload.showVegetationLayer;
          }
          if (payload.showDifferenceMap !== undefined) {
            showDifferenceMap = payload.showDifferenceMap;
          }
          // Capture baseline command
          if (payload.captureBaseline) {
            const somData = readFieldFromSAB('som');
            if (somData) {
              somBaseline = new Float32Array(somData);
              console.log('[Render] SOM baseline captured on user request');
              postMessage({ type: 'baseline-captured' });
            }
          }
        }
        break;

      case 'camera-sync':
        // ORACLE-004: Direct Matrix Injection
        // Bypasses GlobeView's internal calculation to solve offset/shearing/lag/polar issues
        if (payload?.viewMatrix && payload?.projectionMatrix && deck && matrixView) {
          // Copy raw matrices to the custom view instance
          // Using Float32Array.set for optimal performance
          matrixView.viewMatrix.set(payload.viewMatrix);
          matrixView.projectionMatrix.set(payload.projectionMatrix);

          // CRITICAL: Force deck.gl to recognize the matrix update
          // We update the views array to trigger a re-render with the new matrices
          deck.setProps({ views: [matrixView] });
          deck.redraw(true); // Force immediate redraw

          // Optional: Log face ID for debugging polar transitions
          if (payload.faceId !== undefined && frameCount % 300 === 0) {
            console.log(`[Camera] CliMA Face: ${payload.faceId} (0=+Z north, 1=-Z south, 2-5=equatorial)`);
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
