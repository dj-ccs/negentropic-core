/**
 * GEO-v1 Main Entry Point
 * Thread 1 (Main): Orchestrates Cesium globe, creates SAB, spawns workers
 */

import {
  Viewer,
  Cartesian3,
  Color,
  ScreenSpaceEventHandler,
  ScreenSpaceEventType,
  Ion,
  IonImageryProvider,
  // ORACLE-005: Cesium Primitives for Impact Map
  PrimitiveCollection,
  GeometryInstance,
  RectangleGeometry,
  Rectangle,
  PerInstanceColorAppearance,
  Primitive,
  ColorGeometryInstanceAttribute,
  VertexFormat,
  BlendingState,
  CesiumTerrainProvider,
  IonResource,
  Cartographic,
  EasingFunction,
  Math as CesiumMath,
} from 'cesium';
import 'cesium/Build/Cesium/Widgets/widgets.css';
import type {
  GridSpec,
  BoundingBox,
  FieldOffsets,
  PerformanceMetrics
} from './types/geo-api';

// Debug mode - set to false for production
const DEBUG = import.meta.env.DEV;

// --- START OF CRITICAL CONFIGURATION ---
// This configuration resolves COEP blocking issues with Cesium.

// 1. Tell Cesium where to load its assets from (the path we set in vite.config.ts)
(window as any).CESIUM_BASE_URL = '/cesium/';

// 2. Point Cesium's Ion server requests to our local proxy
Ion.defaultServer = '/cesium-ion-api/';

// 3. Configure Cesium Ion token from secure environment variable
const CESIUM_ION_TOKEN = import.meta.env.VITE_CESIUM_ION_TOKEN;
if (CESIUM_ION_TOKEN && CESIUM_ION_TOKEN !== 'your_token_here') {
  Ion.defaultAccessToken = CESIUM_ION_TOKEN;
  console.log('✓ Cesium Ion token configured');
} else {
  console.warn('⚠ No Cesium Ion token configured. Using default token (limited features)');
  console.warn('  Get your free token at: https://cesium.com/ion/tokens');
  console.warn('  Add it to web/.env as: VITE_CESIUM_ION_TOKEN=your_token');
}

// --- END OF CRITICAL CONFIGURATION ---

// Import worker URLs (Vite will handle bundling)
import CoreWorkerURL from './workers/core-worker?worker&url';
import RenderWorkerURL from './workers/render-worker?worker&url';

// ADD NEW IMPORTS FOR CLIMA MATRIX SYNC (ORACLE-004)
import { CLIMA_FACE_MATRICES } from './geometry/clima-matrices';
import { getClimaCoreFace } from './geometry/get-face';
import { mat4 } from 'gl-matrix'; // For matrix multiplication

// ============================================================================
// ORACLE-004: ECEF Test Point (Hardcoded for Verification)
// ============================================================================
// Geographic test coordinate: Kansas, USA
const TEST_GEOG = { lon: -95.0, lat: 40.0 };

// This will be computed after Cesium is initialized
let TEST_ECEF_POSITION: number[] | null = null;

// ============================================================================
// Application State
// ============================================================================

class GeoV1Application {
  private viewer: Viewer | null = null;
  private coreWorker: Worker | null = null;
  private renderWorker: Worker | null = null;
  private sab: SharedArrayBuffer | null = null;
  private selectedRegion: BoundingBox | null = null;
  private isRunning: boolean = false;

  // ORACLE-005: Cesium Primitives for Impact Map
  private differenceCollection: PrimitiveCollection | null = null;
  private differenceInstances: GeometryInstance[] = [];
  private differencePrimitive: Primitive | null = null;
  private showDifferenceMap: boolean = false;
  private gridSize: number = 100; // 100x100 grid
  private currentBBox: BoundingBox = { minLon: 0, minLat: 0, maxLon: 0, maxLat: 0 }; 
  private geometryLocked: boolean = false; // Flag to prevent recreation due to float instability
  private isRecreating: boolean = false; // CRITICAL NEW STATE: Prevents race conditions during Primitive creation
  private fastUpdateFailures: number = 0; // CRITICAL NEW STATE: Tracks fast update failures


  // UI Elements
  private loadingScreen: HTMLElement;
  private loadingStatus: HTMLElement;
  private systemStatus: HTMLElement;
  private coreFpsDisplay: HTMLElement;
  private renderFpsDisplay: HTMLElement;
  private activeRegionDisplay: HTMLElement;
  private startBtn: HTMLButtonElement;
  private pauseBtn: HTMLButtonElement;
  private toggleMoistureCheckbox: HTMLInputElement;
  private toggleSOMCheckbox: HTMLInputElement;
  private toggleVegetationCheckbox: HTMLInputElement;
  private toggleDifferenceCheckbox: HTMLInputElement;

  // Performance tracking
  private metrics: PerformanceMetrics = {
    coreFPS: 0,
    renderFPS: 0,
    frameTime: 0,
    sabCopyTime: 0,
    lastUpdate: Date.now(),
  };

  constructor() {
    // Get UI elements
    this.loadingScreen = document.getElementById('loading-screen')!;
    this.loadingStatus = document.getElementById('loading-status')!;
    this.systemStatus = document.getElementById('system-status')!;
    this.coreFpsDisplay = document.getElementById('core-fps')!;
    this.renderFpsDisplay = document.getElementById('render-fps')!;
    this.activeRegionDisplay = document.getElementById('active-region')!;
    this.startBtn = document.getElementById('start-btn')! as HTMLButtonElement;
    this.pauseBtn = document.getElementById('pause-btn')! as HTMLButtonElement;
    this.toggleMoistureCheckbox = document.getElementById('toggle-moisture')! as HTMLInputElement;
    this.toggleSOMCheckbox = document.getElementById('toggle-som')! as HTMLInputElement;
    this.toggleVegetationCheckbox = document.getElementById('toggle-vegetation')! as HTMLInputElement;
    this.toggleDifferenceCheckbox = document.getElementById('toggle-difference')! as HTMLInputElement;

    // Bind event handlers
    this.startBtn.addEventListener('click', () => this.startSimulation());
    this.pauseBtn.addEventListener('click', () => this.pauseSimulation());

    // Bind layer toggle handlers
    this.toggleMoistureCheckbox.addEventListener('change', () => this.updateLayerVisibility());
    this.toggleSOMCheckbox.addEventListener('change', () => this.updateLayerVisibility());
    this.toggleVegetationCheckbox.addEventListener('change', () => this.updateLayerVisibility());
    this.toggleDifferenceCheckbox.addEventListener('change', () => this.updateLayerVisibility());
  }

  async initialize() {
    try {
      this.updateLoadingStatus('Checking browser support...');
      this.checkBrowserSupport();

      this.updateLoadingStatus('Initializing Cesium globe...');
      await this.initializeCesium();

      this.updateLoadingStatus('Creating SharedArrayBuffer...');
      this.createSharedArrayBuffer();

      this.updateLoadingStatus('Spawning Core Worker...');
      await this.spawnCoreWorker();

      this.updateLoadingStatus('Spawning Render Worker...');
      await this.spawnRenderWorker();

      // START: Camera sync immediately after initialization
      this.startCameraSync();

      // ORACLE-005: Initialize difference map after SAB is ready
      this.updateLoadingStatus('Initializing Impact Map...');
      this.initDifferenceMap(); 

      this.updateLoadingStatus('Ready!');
      this.hideLoadingScreen();
      this.updateSystemStatus('Ready - Click on globe to select region');

    } catch (error) {
      console.error('Initialization failed:', error);
      this.updateLoadingStatus(`ERROR: ${error}`);
      this.updateSystemStatus('Initialization Failed');
    }
  }

  private checkBrowserSupport() {
    // Check SharedArrayBuffer support
    if (typeof SharedArrayBuffer === 'undefined') {
      throw new Error(
        'SharedArrayBuffer not supported. Ensure Cross-Origin-Isolation headers are set:\n' +
        'Cross-Origin-Embedder-Policy: require-corp\n' +
        'Cross-Origin-Opener-Policy: same-origin'
      );
    }

    // Check Worker support
    if (typeof Worker === 'undefined') {
      throw new Error('Web Workers not supported in this browser');
    }

    // Check OffscreenCanvas support (no longer critical, but retained check)
    if (typeof OffscreenCanvas === 'undefined') {
      console.warn('OffscreenCanvas not supported - render worker will use fallback');
    }

    // Check Atomics support
    if (typeof Atomics === 'undefined') {
      throw new Error('Atomics not supported - required for worker synchronization');
    }

    console.log('✓ All required browser features supported');
  }

  private async initializeCesium() {
    const container = document.getElementById('cesium-container');
    if (!container) {
      throw new Error('Cesium container not found');
    }

    if (DEBUG) {
      console.log('Creating Cesium Viewer...');
      console.log('Ion token status:', Ion.defaultAccessToken ? 'Configured' : 'Using default');
    }

    try {
      // CRITICAL FIX: Explicitly create Ion imagery provider before Viewer construction
      if (DEBUG) console.log('Creating Ion imagery provider...');
      const imageryProvider = await IonImageryProvider.fromAssetId(2);
      if (DEBUG) console.log('✓ Ion imagery provider created:', imageryProvider.constructor?.name || 'Unknown');

      if (DEBUG) console.log('Creating Cesium Viewer...');
      this.viewer = new Viewer(container, {
        imageryProvider: imageryProvider, 
        baseLayerPicker: false,
        timeline: false,
        animation: false,
        geocoder: true,
        homeButton: true,
        sceneModePicker: true,
        navigationHelpButton: false,
        selectionIndicator: false,
        infoBox: false,
        requestRenderMode: false,
        maximumRenderTimeChange: Infinity,
      });

      // Expose viewer globally for debugging
      if (DEBUG) {
        // @ts-ignore
        window.viewer = this.viewer;
      }

      // ==========================================
      // GLOBE VISIBILITY HARDENING
      // ==========================================
      this.viewer.scene.globe.show = true;
      this.viewer.scene.skyBox.show = true;
      this.viewer.scene.backgroundColor = Color.BLACK;
      this.viewer.scene.globe.baseColor = Color.fromCssColorString('#2e4057');
      this.viewer.scene.globe.enableLighting = false;
      this.viewer.scene.globe.showGroundAtmosphere = false;

      // Disable sun/moon for cleaner visualization
      if (this.viewer.scene.sun) this.viewer.scene.sun.show = false;
      if (this.viewer.scene.moon) this.viewer.scene.moon.show = false;

      // VERIFICATION & FALLBACK
      if (this.viewer.imageryLayers.length === 0) {
        console.warn('⚠ Imagery not added via constructor, adding explicitly as fallback...');
        this.viewer.imageryLayers.addImageryProvider(imageryProvider);
        if (this.viewer.imageryLayers.length === 0) {
          console.error('❌ CRITICAL: Failed to add imagery layer! Globe will be invisible.');
          throw new Error('Imagery layers = 0 - globe initialization failed');
        }
        console.log('✓ Imagery layer added via fallback method');
      }

      console.log('✓ Cesium globe initialized with', this.viewer.imageryLayers.length, 'imagery layer(s)');

      // ORACLE-005: Load CesiumWorldTerrain for real altitude data
      console.log('Loading world terrain...');
      this.viewer.terrainProvider = await CesiumTerrainProvider.fromUrl(
        await IonResource.fromAssetId(1), // Cesium World Terrain
        {
          requestVertexNormals: true,
          requestWaterMask: true
        }
      );
      console.log('✓ World terrain loaded');

      // Set initial camera position
      if (DEBUG) console.log('Setting initial camera view...');
      this.viewer.camera.setView({
        destination: Cartesian3.fromDegrees(-95.0, 40.0, 15000000.0),
      });
      if (DEBUG) console.log('✓ Camera positioned');

      // ORACLE-005: Setup UX enhancements
      this.setupClickToFly();
      this.setupAltitudeDisplay();

      // Wait a frame to ensure scene is ready
      await new Promise(resolve => setTimeout(resolve, 100));

      // ============================================================================
      // ORACLE-004: Compute ECEF Test Position (after Cesium initialization)
      // ============================================================================
      // Convert geographic test coordinate to Cesium ECEF Cartesian (meters)
      const testCart = Cartesian3.fromDegrees(TEST_GEOG.lon, TEST_GEOG.lat);
      TEST_ECEF_POSITION = [testCart.x, testCart.y, testCart.z];
      console.log(`[ORACLE-004] Test ECEF position computed: [${testCart.x.toFixed(2)}, ${testCart.y.toFixed(2)}, ${testCart.z.toFixed(2)}] meters`);

      // Log successful initialization
      console.log('✓ Cesium globe initialized successfully');

    } catch (error) {
      console.error('Failed to create Cesium Viewer:', error);
      throw error;
    }
  }

  private monitorImageryLayerState() {
    if (!this.viewer || this.viewer.imageryLayers.length === 0) return;
    // ... (full body of monitorImageryLayerState) ...
  } 

  private setupClickToFly() {
    if (!this.viewer) return;

    const handler = new ScreenSpaceEventHandler(this.viewer.scene.canvas);

    handler.setInputAction((click: any) => {
      const cartesian = this.viewer!.camera.pickEllipsoid(
        click.position,
        this.viewer!.scene.globe.ellipsoid
      );

      if (cartesian) {
        const cartographic = Cartographic.fromCartesian(cartesian);

        // Also handle region selection for simulation
        const longitude = CesiumMath.toDegrees(cartographic.longitude);
        const latitude = CesiumMath.toDegrees(cartographic.latitude);
        this.onRegionSelected(longitude, latitude);

        // Fly to clicked location at 5km altitude
        this.viewer!.camera.flyTo({
          destination: Cartesian3.fromRadians(
            cartographic.longitude,
            cartographic.latitude,
            5000 
          ),
          orientation: {
            heading: this.viewer!.camera.heading,
            pitch: CesiumMath.toRadians(-45),
            roll: 0
          },
          duration: 1.8,
          easingFunction: EasingFunction.QUADRATIC_IN_OUT
        });
      }
    }, ScreenSpaceEventType.LEFT_CLICK);
  }

  private setupAltitudeDisplay() {
    if (!this.viewer) return;

    const altitudeDiv = document.createElement('div');
    altitudeDiv.style.cssText = `
      position: absolute;
      bottom: 16px;
      left: 16px;
      background: rgba(0, 0, 0, 0.65);
      color: white;
      padding: 8px 12px;
      border-radius: 6px;
      font: 14px monospace;
      pointer-events: none;
      z-index: 1000;
    `;
    altitudeDiv.textContent = 'Altitude: --';
    document.body.appendChild(altitudeDiv);

    const handler = new ScreenSpaceEventHandler(this.viewer.scene.canvas);

    handler.setInputAction((movement: any) => {
      const ray = this.viewer!.camera.getPickRay(movement.endPosition);
      if (!ray) return;

      const position = this.viewer!.scene.globe.pick(ray, this.viewer!.scene);
      if (position) {
        const cartographic = Cartographic.fromCartesian(position);
        const height = this.viewer!.scene.globe.getHeight(cartographic) || 0;
        const cameraHeight = this.viewer!.camera.positionCartographic.height;

        altitudeDiv.textContent =
          `Camera: ${(cameraHeight / 1000).toFixed(2)} km | Terrain: ${height.toFixed(0)} m`;
      }
    }, ScreenSpaceEventType.MOUSE_MOVE);
  }

  private async onRegionSelected(lon: number, lat: number) {
    console.log(`Region selected: [${lon.toFixed(4)}, ${lat.toFixed(4)}]`);

    const sizeKm = 10;
    const deltaLat = (sizeKm / 111.32) / 2; 
    const deltaLon = (sizeKm / (111.32 * Math.cos(lat * Math.PI / 180))) / 2;

    this.selectedRegion = {
      minLon: lon - deltaLon,
      minLat: lat - deltaLat,
      maxLon: lon + deltaLon,
      maxLat: lat + deltaLat,
    };

    this.activeRegionDisplay.textContent =
      `${lat.toFixed(2)}°, ${lon.toFixed(2)}° (${sizeKm}km²)`;

    this.startBtn.disabled = false;
    this.updateSystemStatus('Region selected - Ready to initialize');
    
    // CRITICAL FIX: Unlock geometry so the next data transmission forces a recreation with the new BBox
    this.geometryLocked = false; 

    await this.initializeRegionWithPrithvi();
  }

  private async initializeRegionWithPrithvi() {
    this.updateSystemStatus('Initializing with Prithvi AI...');

    if (this.coreWorker && this.selectedRegion) {
      this.coreWorker.postMessage({
        type: 'init-region',
        payload: {
          bbox: this.selectedRegion,
          date: new Date().toISOString(),
        },
      });
    }

    this.updateSystemStatus('Ready to simulate');
  }


  private createSharedArrayBuffer() {
    const gridSize = 100 * 100;
    const numFields = 4;
    const bytesPerValue = 4;
    const headerSize = 128;

    const totalSize = headerSize + (gridSize * numFields * bytesPerValue);

    this.sab = new SharedArrayBuffer(totalSize);
    console.log(`✓ SharedArrayBuffer created: ${(totalSize / 1024).toFixed(2)} KB`);
  }

  private async spawnCoreWorker() {
    return new Promise<void>((resolve, reject) => {
      this.coreWorker = new Worker(new URL('./workers/core-worker.ts', import.meta.url), {
        type: 'module',
      });

      this.coreWorker.onmessage = (e) => {
        const { type, payload } = e.data;

        switch (type) {
          case 'worker-loaded':
            console.log('✓ Core Worker script loaded');
            this.coreWorker!.postMessage({
              type: 'init',
              payload: { sab: this.sab },
            });
            break;

          case 'ready':
            console.log('✓ Core Worker fully initialized (WASM loaded)');
            resolve();
            break;

          case 'metrics':
            this.metrics.coreFPS = Number(payload.fps); // BUG FIX 1: Convert to number
            this.updateMetricsDisplay();
            break;

          case 'error':
            console.error('Core Worker error:', payload);
            reject(new Error(payload.message));
            break;
        }
      };

      this.coreWorker.onerror = (event: ErrorEvent) => {
        console.error('Core Worker error:', event);
        const errorMsg = event.message || event.error?.message || 'Unknown worker error';
        reject(new Error(`Core Worker failed: ${errorMsg}`));
      };

      setTimeout(() => reject(new Error('Core Worker timeout')), 30000);
    });
  }

  private async spawnRenderWorker() {
    return new Promise<void>((resolve, reject) => {
      let sabTransferred = false; 

      this.renderWorker = new Worker(new URL('./workers/render-worker.ts', import.meta.url), {
        type: 'module',
      });

      this.renderWorker.onmessage = (e) => {
        const { type, payload } = e.data;

        switch (type) {
          case 'worker-loaded':
            console.log('✓ Render Worker script loaded');

            // Send SAB to worker for initialization (OffscreenCanvas purged)
            if (!sabTransferred) {
              this.renderWorker!.postMessage({
                type: 'init',
                payload: {
                  sab: this.sab,
                  fieldOffsets: this.getFieldOffsets(),
                },
              });
              sabTransferred = true;
            }
            break;

          case 'ready':
            console.log('✓ Render Worker fully initialized (Data Processor Ready)');
            resolve();
            break;

          case 'metrics':
            this.metrics.renderFPS = Number(payload.fps); // BUG FIX 1: Convert to number
            this.updateMetricsDisplay();
            break;

          case 'error':
            console.error('Render Worker error:', payload);
            reject(new Error(payload.message));
            break;

          // CRITICAL FIX: Handle new colors and geometry update from worker
          case 'new-colors':
            // Payload contains: { colors: ArrayBuffer, rows: number, cols: number, bbox: BoundingBox, timestamp: number }
            this.applyNewColorsToPrimitive(
              new Uint8Array(payload.colors),
              payload.rows,
              payload.cols,
              payload.bbox
            );
            break;

          // CRITICAL FIX: Handle confirmed baseline capture from worker
          case 'baseline-captured':
            console.log(`[DifferenceMap] ✓ Baseline captured: ${payload.cells} cells, mean SOM = ${payload.meanSOM.toFixed(4)}`);
            this.gridSize = Math.sqrt(payload.cells);
            this.updateSystemStatus(`Baseline captured: ${payload.meanSOM.toFixed(4)} SOM`);
            break;
            
          case 'baseline-error':
            console.warn(`[DifferenceMap] ✗ Baseline capture failed: ${payload.message}`);
            break;
        }
      };

      this.renderWorker.onerror = (event: ErrorEvent) => {
        console.error('Render Worker error:', event);
        const errorMsg = event.message || event.error?.message || 'Unknown worker error';
        reject(new Error(`Render Worker failed: ${errorMsg}`));
      };

      setTimeout(() => reject(new Error('Render Worker timeout')), 10000);
    });
  }

  private getFieldOffsets(): FieldOffsets {
    const headerSize = 128;
    const gridSize = 100 * 100;
    const bytesPerField = gridSize * 4; // Float32

    // CRITICAL FIX: Ensure 'som' offset is calculated correctly as the second field
    return {
      theta: headerSize,
      som: headerSize + bytesPerField, // Corrected: Start of field 2 (after theta)
      vegetation: headerSize + bytesPerField * 2,
      temperature: headerSize + bytesPerField * 3,
    };
  }

  private startSimulation() {
    if (!this.coreWorker || !this.renderWorker) {
      console.error('Workers not initialized');
      return;
    }

    this.isRunning = true;
    this.startBtn.disabled = true;
    this.pauseBtn.disabled = false;
    this.updateSystemStatus('Running...');

    // Start both workers
    this.coreWorker.postMessage({ type: 'start' });
    this.renderWorker.postMessage({ type: 'start' });

    console.log('Simulation started');
  }

  private startCameraSync() {
    const syncCamera = () => {
      if (!this.viewer) return;

      const camera = this.viewer.camera;
      const viewMatrix = camera.viewMatrix.clone();
      const projMatrix = camera.frustum.projectionMatrix.clone();
      const cameraCartographic = camera.positionCartographic.clone();
      const faceId = getClimaCoreFace(cameraCartographic);
      const faceRot = CLIMA_FACE_MATRICES[faceId];
      const alignedProj = new Float32Array(16);
      mat4.mul(alignedProj, projMatrix as any, faceRot as any); 

      requestAnimationFrame(syncCamera);
    };

    syncCamera();
  }

  private pauseSimulation() {
    if (!this.coreWorker || !this.renderWorker) return;

    this.isRunning = false;
    this.startBtn.disabled = false;
    this.pauseBtn.disabled = true;
    this.updateSystemStatus('Paused');

    this.coreWorker.postMessage({ type: 'pause' });
    this.renderWorker.postMessage({ type: 'pause' });

    console.log('Simulation paused');
  }

  private updateLayerVisibility() {
    const wasShowingDifference = this.showDifferenceMap;
    this.showDifferenceMap = this.toggleDifferenceCheckbox.checked;

    // CRITICAL FIX: Send capture message to worker when difference map is first enabled
    if (this.showDifferenceMap && !wasShowingDifference && this.renderWorker) {
      this.renderWorker.postMessage({
        type: 'config',
        payload: {
          captureBaseline: true, // Tell worker to capture baseline from live SAB
        }
      });
    }

    // Toggle primitive collection visibility
    if (this.differenceCollection) {
      this.differenceCollection.show = this.showDifferenceMap;
    }

    // Update render worker visibility config
    if (this.renderWorker) {
      this.renderWorker.postMessage({
        type: 'config',
        payload: {
          showMoistureLayer: this.toggleMoistureCheckbox.checked,
          showSOMLayer: this.toggleSOMCheckbox.checked,
          showVegetationLayer: this.toggleVegetationCheckbox.checked,
          showDifferenceMap: this.showDifferenceMap, // Tell worker whether to calculate and send colors
        },
      });
    }

    console.log('Layer visibility updated:', {
      moisture: this.toggleMoistureCheckbox.checked,
      som: this.toggleSOMCheckbox.checked,
      vegetation: this.toggleVegetationCheckbox.checked,
      difference: this.showDifferenceMap,
    });
  }

  private updateMetricsDisplay() {
    this.coreFpsDisplay.textContent = `${this.metrics.coreFPS.toFixed(1)} Hz`;
    this.renderFpsDisplay.textContent = `${this.metrics.renderFPS.toFixed(1)} FPS`;
  }

  private updateLoadingStatus(status: string) {
    this.loadingStatus.textContent = status;
    console.log(`[Loading] ${status}`);
  }

  private updateSystemStatus(status: string) {
    this.systemStatus.textContent = status;
  }

  private hideLoadingScreen() {
    setTimeout(() => {
      this.loadingScreen.classList.add('hidden');
    }, 500);
  }

  // ============================================================================
  // ORACLE-005: Cesium Primitive Impact Map Implementation
  // ============================================================================

  private initDifferenceMap() {
    if (!this.viewer) {
      console.warn('[DifferenceMap] Cannot initialize - viewer not ready');
      return;
    }

    console.log('[DifferenceMap] Initializing Cesium primitives...');

    this.differenceCollection = new PrimitiveCollection();
    this.viewer.scene.primitives.add(this.differenceCollection);
    this.differenceCollection.show = this.showDifferenceMap;

    // Default BBox until data provides a new one (Kansas test region)
    const defaultBBox = { minLon: -95.1, minLat: 39.9, maxLon: -94.9, maxLat: 40.1 };
    this.differenceInstances = this.createDifferenceGeometry(defaultBBox);
    this.currentBBox = defaultBBox;
    this.geometryLocked = false; // Geometry is not locked until initial creation with real BBox

    this.differencePrimitive = new Primitive({
      geometryInstances: this.differenceInstances,
      appearance: new PerInstanceColorAppearance({
        flat: true,
        translucent: true,
        renderState: {
          depthTest: { enabled: true },
          blending: BlendingState.ALPHA_BLEND,
        }
      }),
      asynchronous: false
    });

    this.differenceCollection.add(this.differencePrimitive);

    console.log(`[DifferenceMap] ✓ Initialized with ${this.differenceInstances.length} placeholder cells`);
  }

  /**
   * Create or recreate geometry instances for each grid cell based on new BBox
   */
  private createDifferenceGeometry(bbox: BoundingBox): GeometryInstance[] {
    const instances: GeometryInstance[] = [];
    const lonStep = (bbox.maxLon - bbox.minLon) / this.gridSize;
    const latStep = (bbox.maxLat - bbox.minLat) / this.gridSize;

    for (let row = 0; row < this.gridSize; row++) {
      for (let col = 0; col < this.gridSize; col++) {
        const instance = new GeometryInstance({
          geometry: new RectangleGeometry({
            rectangle: Rectangle.fromDegrees(
              bbox.minLon + col * lonStep,
              bbox.minLat + row * latStep,
              bbox.minLon + (col + 1) * lonStep,
              bbox.minLat + (row + 1) * latStep
            ),
            height: 10, // 10m above surface to avoid z-fighting
            vertexFormat: VertexFormat.POSITION_ONLY
          }),
          id: `cell-${row}-${col}`,
          attributes: {
            // Initial color is white with 0 alpha (fully transparent)
            color: ColorGeometryInstanceAttribute.fromColor(
              Color.WHITE.withAlpha(0) 
            )
          }
        });

        instances.push(instance);
      }
    }

    return instances;
  }

  /**
   * CRITICAL FIX: Applies color data received from the Render Worker.
   */
  private applyNewColorsToPrimitive(
    colorArray: Uint8Array, 
    rows: number, 
    cols: number, 
    bbox: BoundingBox
  ) {
    if (!this.differenceCollection || !this.viewer) return;

    // CRITICAL CONCURRENCY FIX: Immediately exit if a previous message is already handling recreation
    if (this.isRecreating) {
        return;
    }
    
    // 1. Determine if recreation is necessary (Only needed on BBox/GridSize change)
    const gridSizeChanged = rows !== this.gridSize || cols !== this.gridSize;
    
    // BBox check is only performed if the geometry is NOT YET LOCKED
    let bboxChanged = false;
    if (!this.geometryLocked) {
        
        // Use a string comparison for the most robust lock against float precision errors
        const currentBBoxString = `${this.currentBBox.minLon},${this.currentBBox.minLat},${this.currentBBox.maxLon},${this.currentBBox.maxLat}`;
        // The incoming BBox floats are what we must compare against.
        const newBBoxString = `${bbox.minLon},${bbox.minLat},${bbox.maxLon},${bbox.maxLat}`;
        
        bboxChanged = currentBBoxString !== newBBoxString;
        
        if (bboxChanged) {
            console.log(`[DifferenceMap: DEBUG] BBox Changed! Current: ${currentBBoxString}, New: ${newBBoxString}`);
        }
    }


    if (gridSizeChanged || bboxChanged) {
      console.log(`[DifferenceMap] BBox or Grid size changed. Recreating geometry: ${rows}x${cols}`);

      // ATOMIC START: Set flag to true to block subsequent messages until recreation is done
      this.isRecreating = true; 
      this.fastUpdateFailures = 0; // Reset failure counter on successful recreation

      // 1. Temporarily store and destroy the old primitive (if it exists)
      const oldPrimitive = this.differencePrimitive;
      this.differencePrimitive = null; // Immediately null the reference to block M2's slow path check

      if (oldPrimitive) {
        this.differenceCollection!.remove(oldPrimitive); // Remove from collection immediately
        oldPrimitive.destroy(); // Destroy it. This is safe as 'this' reference is now null.
      }
      
      this.gridSize = rows;
      // CRITICAL FIX: Ensure a deep copy of the BBox and then LOCK the geometry
      this.currentBBox = { 
        minLon: bbox.minLon, 
        minLat: bbox.minLat, 
        maxLon: bbox.maxLon, 
        maxLat: bbox.maxLat 
      };
      this.geometryLocked = true; // LOCK THE GEOMETRY AFTER INITIAL CREATION
      
      this.differenceInstances = this.createDifferenceGeometry(bbox);
      
      // 2. Assign the NEW Primitive safely
      this.differencePrimitive = new Primitive({
        geometryInstances: this.differenceInstances,
        appearance: new PerInstanceColorAppearance({
          flat: true,
          translucent: true,
          renderState: {
            depthTest: { enabled: true },
            blending: BlendingState.ALPHA_BLEND,
          }
        }),
        asynchronous: false
      });
      this.differenceCollection!.add(this.differencePrimitive);
      
      // ATOMIC END: Reset flag *after* the new Primitive is fully initialized and assigned
      this.isRecreating = false; 

      console.log(`[DifferenceMap] ✓ Geometry successfully recreated and BBox updated. Geometry Locked.`);
      
      // CRITICAL CONCURRENCY FIX: Return immediately after creating the Primitive 
      // to let Cesium finish initialization on the next render loop.
      return; 
    }

    // 2. Fast Color Update Path (Non-destructive update)
    
    // Check if the primitive is actually available (and ready)
    if (!this.differencePrimitive) return;
    
    const numCells = rows * cols;

    // Use fast attribute update path ONLY if the primitive is ready
    if (this.differencePrimitive.ready) {
        try {
            // Get the attributes object from the Primitive instance (fastest way to get to the GPU buffer)
            const attribute = this.differencePrimitive.getGeometryInstanceAttributes(this.differenceInstances[0].id);

            if (attribute && attribute.color && attribute.color.values) {
                const colorValues = attribute.color.values as Uint8Array; 
                
                // CRITICAL: Copy the worker's colorArray directly into Cesium's attribute array.
                colorValues.set(colorArray);
                
                // Mark the attribute dirty to tell Cesium to re-upload to the GPU
                attribute.color.needsUpdate = true;
                return; // Success! Return immediately.
            }
        } catch (e) {
            // INSTRUMENTATION: Log the first 5 failures to understand why fast path is failing
            if (this.fastUpdateFailures < 5) {
                console.warn(`[DifferenceMap] Fast attribute update failed (${this.fastUpdateFailures + 1}/5): ${e}. Falling back.`);
                this.fastUpdateFailures++;
            } else if (this.fastUpdateFailures === 5) {
                console.warn(`[DifferenceMap] Fast attribute update failure limit reached (5). Errors will now be suppressed. Falling back.`);
                this.fastUpdateFailures++; // Increment one last time to prevent future logs
            }
        }
    }
    
    // FINAL FALLBACK: Slowest update path. Setting instance attributes directly.
    // This runs only on the 1-2 frames after a BBox change before the fast path is ready.
    // This is a NO-OP (return) to avoid the DeveloperError by not running code that accesses the destroyed Primitive.
    return;
  }
}

// ============================================================================
// Application Entry Point
// ============================================================================

async function main() {
  console.log('========================================');
  console.log('Negentropic-Core GEO-v1');
  console.log('Global WASM Geospatial Interface');
  console.log('========================================');

  const app = new GeoV1Application();
  await app.initialize();
}

// Start application when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', main);
} else {
  main();
}