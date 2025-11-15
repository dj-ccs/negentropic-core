/**
 * Core Worker (Thread 2)
 * Loads negentropic-core.wasm, runs simulation at 10 Hz, writes to SAB
 */

import type {
  WASMModule,
  CoreWorkerMessage,
  GridSpec,
  RunConfig,
  SABHeader,
} from '../types/geo-api';

// ============================================================================
// Worker State
// ============================================================================

let wasmModule: WASMModule | null = null;
let sab: SharedArrayBuffer | null = null;
let simHandle: number = 0;
let isRunning: boolean = false;
let targetFPS: number = 10; // 10 Hz simulation frequency
let frameCount: number = 0;
let lastFPSUpdate: number = Date.now();

// SAB configuration
const SAB_HEADER_SIZE = 128;
const SAB_SIGNAL_INDEX = 0;

// Grid configuration
let gridRows: number = 100;
let gridCols: number = 100;

// ============================================================================
// WASM Module Loading
// ============================================================================

async function loadWASM(): Promise<WASMModule> {
  try {
    // Load the Emscripten-generated module from public directory
    // Module workers don't support importScripts, so we use fetch + eval
    const scriptUrl = '/wasm/negentropic_core.js';

    console.log('Fetching WASM loader script...');
    const response = await fetch(scriptUrl);
    if (!response.ok) {
      throw new Error(`Failed to fetch ${scriptUrl}: ${response.status}`);
    }

    const scriptText = await response.text();

    // Evaluate the script in the worker's global scope
    // This will define the createNegentropic factory function
    // eslint-disable-next-line no-eval
    (0, eval)(scriptText);

    // @ts-ignore - createNegentropic is defined globally by Emscripten
    const ModuleFactory = self.createNegentropic;

    if (!ModuleFactory || typeof ModuleFactory !== 'function') {
      console.error('Available globals:', Object.keys(self).filter(k => !k.startsWith('_')));
      throw new Error('createNegentropic factory function not found. Check Emscripten build configuration.');
    }

    console.log('Initializing WASM module with createNegentropic()...');

    // Create a promise that resolves when the runtime is fully initialized
    const module = await new Promise<WASMModule>((resolve, reject) => {
      ModuleFactory({
        locateFile: (path: string) => {
          if (path.endsWith('.wasm')) {
            return '/wasm/negentropic_core.wasm';
          }
          return path;
        },
        print: (text: string) => console.log('[WASM]', text),
        printErr: (text: string) => console.error('[WASM]', text),
        onRuntimeInitialized: function() {
          console.log('[WASM] Runtime initialized');
          // 'this' is the Module object
          resolve(this as WASMModule);
        },
        onAbort: (what: any) => {
          reject(new Error(`WASM aborted: ${what}`));
        },
      }).catch(reject);
    });

    console.log('✓ WASM module loaded in Core Worker');
    console.log('  - HEAPU8 available:', !!module.HEAPU8);
    console.log('  - _malloc available:', !!module._malloc);
    return module as WASMModule;

  } catch (error) {
    console.error('Failed to load WASM module:', error);
    throw error;
  }
}

// ============================================================================
// Simulation Initialization
// ============================================================================

function initializeSimulation() {
  if (!wasmModule) {
    throw new Error('WASM module not loaded');
  }

  // Create configuration JSON
  const config = {
    num_entities: 0,
    num_scalar_fields: gridRows * gridCols,
    grid_width: gridCols,
    grid_height: gridRows,
    grid_depth: 1,
    dt: 1.0 / targetFPS, // timestep in seconds
    precision_mode: 1,
    integrator_type: 0,
    enable_atmosphere: true,
    enable_hydrology: true,
    enable_soil: true,
  };

  const configJSON = JSON.stringify(config);

  // Allocate string in WASM memory
  const configPtr = allocateString(configJSON);

  try {
    // Create simulation
    simHandle = wasmModule._neg_create(configPtr);

    if (simHandle === 0) {
      throw new Error('neg_create returned NULL handle');
    }

    console.log(`✓ Simulation initialized (handle: ${simHandle})`);

  } finally {
    // Free the config string
    wasmModule._free(configPtr);
  }
}

// ============================================================================
// Simulation Loop
// ============================================================================

function simulationLoop() {
  if (!isRunning || !wasmModule || !sab) {
    return;
  }

  const dt = 1.0 / targetFPS;

  try {
    // Step the simulation
    const result = wasmModule._neg_step(simHandle, dt);

    if (result !== 0) {
      console.error(`neg_step failed with code ${result}`);
      isRunning = false;
      postMessage({ type: 'error', payload: { code: result } });
      return;
    }

    // Get binary state
    const stateSize = wasmModule._neg_get_state_binary_size(simHandle);
    const stateBuffer = wasmModule._malloc(stateSize);

    try {
      const bytesWritten = wasmModule._neg_get_state_binary(simHandle, stateBuffer, stateSize);

      if (bytesWritten > 0) {
        // Copy state to SharedArrayBuffer
        copyStateToSAB(stateBuffer, bytesWritten);

        // Signal Render Worker that new data is ready
        signalRenderWorker();
      }

    } finally {
      wasmModule._free(stateBuffer);
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
    console.error('Simulation loop error:', error);
    isRunning = false;
    postMessage({ type: 'error', payload: { message: String(error) } });
    return;
  }

  // Schedule next frame
  const frameTime = 1000 / targetFPS;
  setTimeout(simulationLoop, frameTime);
}

// ============================================================================
// SharedArrayBuffer Operations
// ============================================================================

function copyStateToSAB(wasmBufferPtr: number, size: number) {
  if (!sab || !wasmModule) return;

  // Get the WASM memory as a typed array view
  const wasmData = new Uint8Array(
    wasmModule.HEAPU8.buffer,
    wasmBufferPtr,
    size
  );

  // Get the SAB as a typed array
  const sabView = new Uint8Array(sab);

  // Copy data (skip header for now, will implement proper format later)
  const dataStart = SAB_HEADER_SIZE;
  const copySize = Math.min(size, sab.byteLength - dataStart);

  sabView.set(wasmData.subarray(0, copySize), dataStart);
}

function signalRenderWorker() {
  if (!sab) return;

  const int32View = new Int32Array(sab);

  // Increment the signal counter
  const oldValue = Atomics.add(int32View, SAB_SIGNAL_INDEX, 1);

  // Notify waiting workers
  Atomics.notify(int32View, SAB_SIGNAL_INDEX, 1);
}

function writeHeader(header: Partial<SABHeader>) {
  if (!sab) return;

  const headerView = new DataView(sab);

  let offset = 4; // Skip first 4 bytes (used for Atomics signal)

  // Version
  headerView.setUint32(offset, header.version || 1, true);
  offset += 4;

  // Epoch
  headerView.setUint32(offset, header.epoch || 0, true);
  offset += 4;

  // Grid dimensions
  headerView.setUint32(offset, header.rows || gridRows, true);
  offset += 4;
  headerView.setUint32(offset, header.cols || gridCols, true);
  offset += 4;

  // Timestamp
  headerView.setFloat64(offset, header.timestamp || Date.now(), true);
  offset += 8;

  // BBox (4 floats)
  if (header.bbox) {
    headerView.setFloat32(offset, header.bbox.minLon, true);
    offset += 4;
    headerView.setFloat32(offset, header.bbox.minLat, true);
    offset += 4;
    headerView.setFloat32(offset, header.bbox.maxLon, true);
    offset += 4;
    headerView.setFloat32(offset, header.bbox.maxLat, true);
    offset += 4;
  }
}

// ============================================================================
// Helper Functions
// ============================================================================

function allocateString(str: string): number {
  if (!wasmModule) {
    throw new Error('WASM module not loaded');
  }

  if (!wasmModule.HEAPU8) {
    console.error('WASM module state:', {
      hasModule: !!wasmModule,
      hasMalloc: !!wasmModule._malloc,
      hasHEAPU8: !!wasmModule.HEAPU8,
      moduleKeys: Object.keys(wasmModule).slice(0, 20),
    });
    throw new Error('WASM module HEAPU8 not available - runtime may not be fully initialized');
  }

  const encoder = new TextEncoder();
  const bytes = encoder.encode(str + '\0'); // Null-terminated

  const ptr = wasmModule._malloc(bytes.length);
  wasmModule.HEAPU8.set(bytes, ptr);

  return ptr;
}

// ============================================================================
// Message Handlers
// ============================================================================

self.onmessage = async (e: MessageEvent<CoreWorkerMessage>) => {
  const { type, payload } = e.data;

  try {
    switch (type) {
      case 'init':
        if (payload?.sab) {
          sab = payload.sab;
          console.log('Core Worker received SAB');

          // Load WASM module
          wasmModule = await loadWASM();

          // Initialize simulation
          initializeSimulation();

          postMessage({ type: 'ready' });
        }
        break;

      case 'init-region':
        // Initialize grid with Prithvi data (placeholder for now)
        if (payload) {
          console.log('Initializing region:', payload.bbox);
          // TODO: Call Prithvi adapter and load initial state into WASM
          writeHeader({
            version: 1,
            epoch: 0,
            rows: gridRows,
            cols: gridCols,
            timestamp: Date.now(),
            bbox: payload.bbox,
          });
        }
        break;

      case 'start':
        if (!isRunning) {
          isRunning = true;
          frameCount = 0;
          lastFPSUpdate = Date.now();
          console.log('Core Worker: Starting simulation loop');
          simulationLoop();
        }
        break;

      case 'pause':
        isRunning = false;
        console.log('Core Worker: Paused');
        break;

      case 'reset':
        if (wasmModule && simHandle !== 0) {
          wasmModule._neg_destroy(simHandle);
          initializeSimulation();
        }
        break;

      default:
        console.warn('Unknown message type:', type);
    }

  } catch (error) {
    console.error('Core Worker error:', error);
    postMessage({
      type: 'error',
      payload: { message: String(error) },
    });
  }
};

// Signal that worker script is loaded (but WASM not yet loaded)
postMessage({ type: 'worker-loaded' });
console.log('Core Worker script loaded, awaiting init...');
