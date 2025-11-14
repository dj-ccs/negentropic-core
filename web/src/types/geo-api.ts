/**
 * GEO-v1 API Type Definitions
 * Based on Part 4 of the research document: Data pipeline and WASM integration
 */

// ============================================================================
// Geographic Coordinate Systems
// ============================================================================

export interface BoundingBox {
  minLon: number;
  minLat: number;
  maxLon: number;
  maxLat: number;
}

export interface Point2D {
  x: number;
  y: number;
}

export interface Point3D extends Point2D {
  z: number;
}

// ============================================================================
// Tile Selection & Grid Specification
// ============================================================================

export interface TileSelection {
  id: string;
  bbox_wgs84: [number, number, number, number]; // [minLon, minLat, maxLon, maxLat]
  pyramid_level: number; // LOD request for terrain/imagery
  target_resolution_m: number; // desired core cell size (m)
  time_span: {
    start_iso: string;
    end_iso: string;
  };
  sources: {
    imagery?: {
      collection: string;
      bands: string[];
    };
    climate?: {
      product: string;
      variables: string[];
    };
    s1?: {
      polarizations: string[];
    };
  };
}

export interface GridSpec {
  grid_id: string;
  crs: string; // EPSG code
  origin_xy: [number, number];
  cellsize_m: number;
  shape_rc: [number, number]; // [rows, cols]
  transform: [number, number, number, number, number, number]; // affine [a,b,c,d,e,f]
}

// ============================================================================
// Initial State & Simulation Configuration
// ============================================================================

export interface VariableDescriptor {
  name: string;
  dtype: 'float32' | 'uint8' | 'int16' | 'float64';
  layout: 'row-major' | 'col-major';
  stats?: {
    min: number;
    max: number;
    mean: number;
    std: number;
  };
}

export interface DataReference {
  type: 'SAB' | 'URL' | 'RTC'; // SharedArrayBuffer, URL, or Real-Time Channel
  key: string;
}

export interface InitState {
  grid_id: string;
  variables: VariableDescriptor[];
  data_ref: DataReference;
}

export interface RunConfig {
  grid_id: string;
  dt_s: number; // timestep in seconds
  steps: number;
  physics: {
    enable_hydrology?: boolean;
    enable_regeneration?: boolean;
    enable_microbial?: boolean;
    enable_atmosphere?: boolean;
  };
}

export interface OutputSlice {
  grid_id: string;
  t_index: number;
  var: string;
  bbox_wgs84: [number, number, number, number];
  data_ref: DataReference;
}

// ============================================================================
// SharedArrayBuffer Protocol
// ============================================================================

export interface SABHeader {
  version: number;
  epoch: number;
  gridID: string;
  bbox: BoundingBox;
  stride: number;
  rows: number;
  cols: number;
  timestamp: number;
}

export const SAB_HEADER_SIZE = 128; // bytes reserved for header
export const SAB_SIGNAL_INDEX = 0; // Atomics signal location

// Field offsets in SharedArrayBuffer
export interface FieldOffsets {
  theta: number; // soil moisture
  som: number; // soil organic matter
  vegetation: number; // vegetation cover
  temperature: number;
  // Add more fields as needed
}

// ============================================================================
// WASM Module Interface
// ============================================================================

export interface WASMModule {
  // Memory management
  _malloc(size: number): number;
  _free(ptr: number): void;

  // API functions (from negentropic.h)
  _neg_create(config_json_ptr: number): number;
  _neg_destroy(sim: number): void;
  _neg_step(sim: number, dt: number): number;
  _neg_get_state_binary(sim: number, buffer: number, max_len: number): number;
  _neg_get_state_binary_size(sim: number): number;
  _neg_get_state_hash(sim: number): bigint;
  _neg_reset_from_binary(sim: number, buffer: number, len: number): number;

  // WASM exports
  HEAPU8: Uint8Array;
  HEAPF32: Float32Array;
  ccall: (name: string, returnType: string, argTypes: string[], args: any[]) => any;
  cwrap: (name: string, returnType: string, argTypes: string[]) => Function;
}

// ============================================================================
// Worker Messages
// ============================================================================

export type WorkerMessageType =
  | 'init'
  | 'start'
  | 'stop'
  | 'step'
  | 'reset'
  | 'config'
  | 'state-update'
  | 'error';

export interface WorkerMessage {
  type: WorkerMessageType;
  payload?: any;
  timestamp?: number;
}

export interface CoreWorkerMessage extends WorkerMessage {
  type: WorkerMessageType;
  payload?: {
    sab?: SharedArrayBuffer;
    gridSpec?: GridSpec;
    runConfig?: RunConfig;
    initState?: InitState;
  };
}

export interface RenderWorkerMessage extends WorkerMessage {
  type: WorkerMessageType;
  payload?: {
    sab?: SharedArrayBuffer;
    offscreenCanvas?: OffscreenCanvas;
    fieldOffsets?: FieldOffsets;
  };
}

// ============================================================================
// Prithvi Model Interface
// ============================================================================

export interface PrithviInput {
  bbox: BoundingBox;
  date: string;
  bands: string[]; // ['Blue', 'Green', 'Red', 'NIRn', 'SWIR1', 'SWIR2']
  resolution: number; // meters
}

export interface PrithviOutput {
  mask: Float32Array; // segmentation mask
  embeddings: Float32Array; // encoder features
  metadata: {
    classes: string[];
    confidence: number;
    timestamp: string;
  };
}

// ============================================================================
// Performance Metrics
// ============================================================================

export interface PerformanceMetrics {
  coreFPS: number; // Physics simulation Hz
  renderFPS: number; // Rendering FPS
  frameTime: number; // ms
  sabCopyTime: number; // ms
  lastUpdate: number; // timestamp
}
