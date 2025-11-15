/**
 * Render Worker (Thread 3)
 * Runs deck.gl on OffscreenCanvas, reads SAB at 60 FPS
 */

import type {
  RenderWorkerMessage,
  FieldOffsets,
  SABHeader,
} from '../types/geo-api';

// Deck.gl modules - loaded dynamically
let Deck: any;
let GridLayer: any;
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
    GridLayer = deckLayers.GridLayer;
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
    // Create a minimal WebGL context for deck.gl
    const gl = canvas.getContext('webgl2', {
      alpha: true,
      antialias: true,
      premultipliedAlpha: false,
    });

    if (!gl) {
      throw new Error('Failed to get WebGL2 context');
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
    });

    console.log('✓ Deck.gl initialized in Render Worker');

  } catch (error) {
    console.error('Failed to initialize deck.gl:', error);
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

  const header: SABHeader = {
    version: headerView.getUint32(offset, true),
    epoch: headerView.getUint32(offset + 4, true),
    gridID: 'default',
    bbox: {
      minLon: headerView.getFloat32(offset + 16, true),
      minLat: headerView.getFloat32(offset + 20, true),
      maxLon: headerView.getFloat32(offset + 24, true),
      maxLat: headerView.getFloat32(offset + 28, true),
    },
    stride: gridCols,
    rows: headerView.getUint32(offset + 8, true),
    cols: headerView.getUint32(offset + 12, true),
    timestamp: headerView.getFloat64(offset + 32, true),
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
  if (!header) return;

  // Convert field data to grid cells for GridLayer
  const gridData = convertFieldToGrid(fieldData, header);

  // Define color scale based on field type
  const [minVal, maxVal] = getFieldRange(currentField);

  const layers = [
    new GridLayer({
      id: 'field-grid',
      data: gridData,
      pickable: true,
      extruded: false,
      cellSize: getCellSize(header),
      getPosition: (d: any) => d.position,
      getFillColor: (d: any) => valueToColor(d.value, minVal, maxVal),
      opacity: 0.7,
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

// Signal that worker is ready
try {
  postMessage({ type: 'ready' });
  console.log('Render Worker initialized');
} catch (error) {
  console.error('Render Worker failed to initialize:', error);
  postMessage({
    type: 'error',
    payload: { message: `Initialization failed: ${String(error)}` }
  });
}
