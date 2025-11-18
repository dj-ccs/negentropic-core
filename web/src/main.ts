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
// For details, see: docs/CESIUM_GUIDE.md - Section 4 "COOP/COEP Integration for SAB"

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
  private somBaseline: Float32Array | null = null;
  private showDifferenceMap: boolean = false;
  private gridSize: number = 100; // 100x100 grid

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
      // This ensures deck.gl layers track the globe even before simulation starts
      this.startCameraSync();

      // ORACLE-005: Initialize difference map after SAB is ready
      this.updateLoadingStatus('Initializing Impact Map...');
      this.initDifferenceMap();
      this.startDifferenceMapLoop();

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

    // Check OffscreenCanvas support
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
      // See docs/CESIUM_GUIDE.md Section "Implementation in GEO-v1" for details
      // Using IonImageryProvider.fromAssetId(2) = Bing Maps Aerial with Labels
      if (DEBUG) console.log('Creating Ion imagery provider...');
      const imageryProvider = await IonImageryProvider.fromAssetId(2);
      if (DEBUG) console.log('✓ Ion imagery provider created:', imageryProvider.constructor?.name || 'Unknown');

      if (DEBUG) console.log('Creating Cesium Viewer...');
      this.viewer = new Viewer(container, {
        imageryProvider: imageryProvider, // Pass provider object directly to constructor
        baseLayerPicker: false,           // Disable UI picker
        timeline: false,
        animation: false,
        geocoder: true,
        homeButton: true,
        sceneModePicker: true,
        navigationHelpButton: false,
        selectionIndicator: false,
        infoBox: false,
        requestRenderMode: false,    // Continuous rendering for 60 FPS
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
      // Force globe visibility and configure appearance
      // Ref: docs/INTERCONNECTION_GUIDE.md Section 3
      this.viewer.scene.globe.show = true;
      this.viewer.scene.skyBox.show = true;
      this.viewer.scene.backgroundColor = Color.BLACK;
      this.viewer.scene.globe.baseColor = Color.fromCssColorString('#2e4057');
      this.viewer.scene.globe.enableLighting = false;
      this.viewer.scene.globe.showGroundAtmosphere = false;

      // Disable sun/moon for cleaner visualization
      if (this.viewer.scene.sun) this.viewer.scene.sun.show = false;
      if (this.viewer.scene.moon) this.viewer.scene.moon.show = false;

      // Log viewer creation success
      if (DEBUG) {
        console.log('✓ Cesium Viewer created');
        console.log('  - Scene mode:', this.viewer.scene.mode);
        console.log('  - Globe enabled:', this.viewer.scene.globe ? 'Yes' : 'No');
        console.log('  - Terrain provider:', this.viewer.terrainProvider?.constructor?.name || 'None');
        console.log('  - Imagery layers:', this.viewer.imageryLayers.length);
      }

      // VERIFICATION & FALLBACK: Check if constructor successfully added the imagery layer
      // In CesiumJS v1.120+, passing imageryProvider to constructor *should* add it,
      // but we verify and add explicitly as fallback if needed
      if (this.viewer.imageryLayers.length === 0) {
        console.warn('⚠ Imagery not added via constructor, adding explicitly as fallback...');
        this.viewer.imageryLayers.addImageryProvider(imageryProvider);

        // If still zero after fallback, this is a critical error
        if (this.viewer.imageryLayers.length === 0) {
          console.error('❌ CRITICAL: Failed to add imagery layer! Globe will be invisible.');
          throw new Error('Imagery layers = 0 - globe initialization failed');
        }
        console.log('✓ Imagery layer added via fallback method');
      } else {
        console.log('✓ Imagery layer added via constructor');
      }

      console.log('✓ Cesium globe initialized with', this.viewer.imageryLayers.length, 'imagery layer(s)');

      // Verify globe is enabled
      if (!this.viewer.scene.globe) {
        console.warn('⚠ Globe is not enabled in the scene!');
      }

      // Monitor and verify imagery layer state
      if (DEBUG && this.viewer.imageryLayers.length > 0) {
        this.monitorImageryLayerState();
      }

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

      // Debug: Detailed visibility diagnostics
      if (DEBUG) {
        console.log('[DEBUG] Globe visibility check:', {
          globeShow: this.viewer.scene.globe.show,
          backgroundColor: this.viewer.scene.backgroundColor,
          canvasWidth: this.viewer.canvas.width,
          canvasHeight: this.viewer.canvas.height,
        });

        const cesiumCanvas = this.viewer.canvas;
        const computedStyle = window.getComputedStyle(cesiumCanvas);
        console.log('[DEBUG] Canvas computed styles:', {
          display: computedStyle.display,
          visibility: computedStyle.visibility,
          opacity: computedStyle.opacity,
          width: computedStyle.width,
          height: computedStyle.height,
        });
      }

    } catch (error) {
      console.error('Failed to create Cesium Viewer:', error);
      throw error;
    }
  }

  /**
   * Monitor imagery layer state for debugging purposes
   * Checks provider readiness and enforces visibility settings
   */
  private monitorImageryLayerState() {
    if (!this.viewer || this.viewer.imageryLayers.length === 0) return;

    const imageryLayer = this.viewer.imageryLayers.get(0);
    const provider = imageryLayer.imageryProvider;

    if (provider) {
      console.log('[DEBUG] Imagery layer details:', {
        show: imageryLayer.show,
        alpha: imageryLayer.alpha,
        ready: imageryLayer.ready,
        providerReady: provider.ready,
        providerType: provider.constructor?.name || 'Unknown',
      });

      // Monitor provider readiness
      if (!provider.ready) {
        console.log('⏳ Waiting for imagery provider to become ready...');
        const startTime = Date.now();
        const checkReady = () => {
          if (provider.ready) {
            console.log(`✓ Imagery provider ready after ${Date.now() - startTime}ms`);
          } else if (Date.now() - startTime < 5000) {
            setTimeout(checkReady, 100);
          } else {
            console.warn('⚠ Imagery provider not ready after 5s timeout');
          }
        };
        checkReady();
      } else {
        console.log('✓ Imagery provider is already ready');
      }

      // Enforce visibility
      if (!imageryLayer.show) {
        console.warn('⚠ Imagery layer show is FALSE - forcing to true');
        imageryLayer.show = true;
      }
      if (imageryLayer.alpha < 1.0) {
        console.warn('⚠ Imagery layer alpha is', imageryLayer.alpha, '- forcing to 1.0');
        imageryLayer.alpha = 1.0;
      }
    } else {
      console.log('⏳ Imagery provider not yet available, monitoring...');
      const startTime = Date.now();
      const checkProvider = () => {
        const layer = this.viewer!.imageryLayers.get(0);
        if (layer && layer.imageryProvider) {
          console.log(`✓ Imagery provider available after ${Date.now() - startTime}ms`);
          console.log('[DEBUG] Provider type:', layer.imageryProvider.constructor?.name || 'Unknown');
        } else if (Date.now() - startTime < 5000) {
          setTimeout(checkProvider, 100);
        } else {
          console.warn('⚠ Imagery provider not available after 5s timeout');
        }
      };
      checkProvider();
    }
  }

  /**
   * ORACLE-005: Click-to-fly camera control
   * Left-click on globe to fly to that location at 5km altitude
   */
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
            5000 // 5km altitude for optimal viewing
          ),
          orientation: {
            heading: this.viewer!.camera.heading,
            pitch: CesiumMath.toRadians(-45), // 45° downward pitch
            roll: 0
          },
          duration: 1.8,
          easingFunction: EasingFunction.QUADRATIC_IN_OUT
        });
      }
    }, ScreenSpaceEventType.LEFT_CLICK);
  }

  /**
   * ORACLE-005: Real-time altitude display
   * Shows camera altitude and terrain height on mouse move
   */
  private setupAltitudeDisplay() {
    if (!this.viewer) return;

    // Create HUD element
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

    // Update on mouse move
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

    // Create a 10km x 10km bounding box centered on click point
    const sizeKm = 10;
    const deltaLat = (sizeKm / 111.32) / 2; // 1 degree ≈ 111.32 km
    const deltaLon = (sizeKm / (111.32 * Math.cos(lat * Math.PI / 180))) / 2;

    this.selectedRegion = {
      minLon: lon - deltaLon,
      minLat: lat - deltaLat,
      maxLon: lon + deltaLon,
      maxLat: lat + deltaLat,
    };

    this.activeRegionDisplay.textContent =
      `${lat.toFixed(2)}°, ${lon.toFixed(2)}° (${sizeKm}km²)`;

    // Enable start button
    this.startBtn.disabled = false;
    this.updateSystemStatus('Region selected - Ready to initialize');

    // TODO: Call Prithvi adapter to initialize landscape state
    // For now, we'll use a placeholder initialization
    await this.initializeRegionWithPrithvi();
  }

  private async initializeRegionWithPrithvi() {
    this.updateSystemStatus('Initializing with Prithvi AI...');

    // TODO: Implement actual Prithvi adapter call
    // For now, send initialization message to Core Worker
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
    // Calculate SAB size for a 100x100 grid with 4 fields (theta, SOM, vegetation, temperature)
    const gridSize = 100 * 100;
    const numFields = 4;
    const bytesPerValue = 4; // Float32
    const headerSize = 128; // bytes for metadata

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
            // Transfer SAB to worker for initialization
            this.coreWorker!.postMessage({
              type: 'init',
              payload: { sab: this.sab },
            });
            // Don't resolve yet - wait for 'ready' after WASM loads
            break;

          case 'ready':
            console.log('✓ Core Worker fully initialized (WASM loaded)');
            resolve();
            break;

          case 'metrics':
            this.metrics.coreFPS = payload.fps;
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

      // Timeout increased to 30s to allow for WASM loading
      setTimeout(() => reject(new Error('Core Worker timeout')), 30000);
    });
  }

  private async spawnRenderWorker() {
    return new Promise<void>((resolve, reject) => {
      let canvasTransferred = false; // Flag to prevent double transfer

      this.renderWorker = new Worker(new URL('./workers/render-worker.ts', import.meta.url), {
        type: 'module',
      });

      this.renderWorker.onmessage = (e) => {
        const { type, payload } = e.data;

        switch (type) {
          case 'worker-loaded':
            console.log('✓ Render Worker script loaded');

            // Transfer SAB and OffscreenCanvas to worker for initialization
            if (!canvasTransferred) {
              const canvas = document.getElementById('overlay-canvas') as HTMLCanvasElement;
              const offscreen = canvas.transferControlToOffscreen();

              this.renderWorker!.postMessage({
                type: 'init',
                payload: {
                  sab: this.sab,
                  offscreenCanvas: offscreen,
                  fieldOffsets: this.getFieldOffsets(),
                  testEcefPosition: TEST_ECEF_POSITION, // ORACLE-004: Send ECEF test position
                },
              }, [offscreen]);

              canvasTransferred = true;
            }
            // Don't resolve yet - wait for 'ready' after deck.gl loads
            break;

          case 'ready':
            console.log('✓ Render Worker fully initialized (deck.gl loaded)');
            resolve();
            break;

          case 'metrics':
            this.metrics.renderFPS = payload.fps;
            this.updateMetricsDisplay();
            break;

          case 'error':
            console.error('Render Worker error:', payload);
            reject(new Error(payload.message));
            break;
        }
      };

      this.renderWorker.onerror = (event: ErrorEvent) => {
        console.error('Render Worker error:', event);
        const errorMsg = event.message || event.error?.message || 'Unknown worker error';
        reject(new Error(`Render Worker failed: ${errorMsg}`));
      };

      // Timeout increased to 30s to allow for deck.gl loading
      setTimeout(() => reject(new Error('Render Worker timeout')), 30000);
    });
  }

  private getFieldOffsets(): FieldOffsets {
    const headerSize = 128;
    const gridSize = 100 * 100;
    const bytesPerField = gridSize * 4; // Float32

    return {
      theta: headerSize,
      som: headerSize + bytesPerField,
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

    // NOTE: Camera sync is already running (started in initialize())

    console.log('Simulation started');
  }

  /**
   * Camera Synchronization Loop (ORACLE-004: CliMA Matrix Injection)
   * Continuously sync Cesium camera state to deck.gl render worker
   * This ensures deck.gl layers move with the globe
   *
   * ARCHITECTURAL CHANGE: Replaces parameter-based synchronization with direct
   * matrix injection. Extracts raw view/projection matrices from Cesium and applies
   * CliMA cubed-sphere face rotation for mathematically perfect alignment.
   */
  private startCameraSync() {
    const syncCamera = () => {
      if (!this.viewer || !this.renderWorker) return;

      // Read Cesium camera state
      const camera = this.viewer.camera;

      // 1. Get raw matrices
      const viewMatrix = camera.viewMatrix.clone(); // Cesium.Matrix4 -> Float32Array
      const projMatrix = camera.frustum.projectionMatrix.clone(); // Cesium.Matrix4

      // 2. Determine CliMA Face and Rotation Matrix
      const cameraCartographic = camera.positionCartographic.clone();
      const faceId = getClimaCoreFace(cameraCartographic);
      const faceRot = CLIMA_FACE_MATRICES[faceId];

      // 3. Apply Face Rotation to Projection Matrix
      // This is the CRITICAL STEP for global alignment (Grok's Fix)
      const alignedProj = new Float32Array(16);
      mat4.mul(alignedProj, projMatrix as any, faceRot as any); // Use gl-matrix mul

      // Send raw matrices to render worker (as flat arrays)
      this.renderWorker.postMessage({
        type: 'camera-sync',
        payload: {
          viewMatrix: Array.from(viewMatrix), // Send raw View Matrix
          projectionMatrix: Array.from(alignedProj), // Send Face-Rotated Projection Matrix
          faceId: faceId // For debug
        },
      });

      // Continue syncing (camera can move even while paused)
      requestAnimationFrame(syncCamera);
    };

    // Start the sync loop
    syncCamera();
  }

  /**
   * Convert Cesium altitude (meters) to deck.gl GlobeView altitude
   *
   * deck.gl GlobeView altitude is relative to viewport height:
   * - altitude = 1.0 means camera is at 1x viewport height above surface
   * - altitude = 1.5 is the default (nice overview of globe)
   * - Smaller values = closer to surface
   * - Larger values = farther from globe
   *
   * Cesium altitude is absolute distance from ellipsoid surface in meters.
   *
   * @param cesiumAltitude - Altitude in meters from Cesium camera
   * @returns Relative altitude for deck.gl GlobeView
   */
  private cesiumAltitudeToGlobeViewAltitude(cesiumAltitude: number): number {
    // Earth radius in meters (WGS84 equatorial radius)
    const EARTH_RADIUS = 6378137;

    // Normalize altitude relative to Earth radius
    // This gives us a scale-independent value
    const normalizedAltitude = cesiumAltitude / EARTH_RADIUS;

    // Map to deck.gl GlobeView altitude range
    // - When viewing whole globe (altitude ~15M meters), we want ~1.5
    // - When zoomed in close (altitude ~1000 meters), we want ~0.01
    //
    // Scale factor: 0.50 (FINAL CALIBRATION to resolve magnitude sync failure)
    // Previous values: 0.65 (too subtle), 0.85 (magnitude mismatch)
    // This lower factor should provide tighter coupling to Cesium's altitude
    const deckAltitude = normalizedAltitude * 0.50;

    // Clamp to reasonable range (0.001 = very close, 10 = very far)
    return Math.max(0.001, Math.min(10, deckAltitude));
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
    // ORACLE-005: Handle Cesium primitive difference map
    const wasShowingDifference = this.showDifferenceMap;
    this.showDifferenceMap = this.toggleDifferenceCheckbox.checked;

    // Capture baseline when difference map is first enabled
    if (this.showDifferenceMap && !wasShowingDifference && !this.somBaseline) {
      this.captureBaseline();
    }

    // Toggle primitive collection visibility
    if (this.differenceCollection) {
      this.differenceCollection.show = this.showDifferenceMap;
    }

    // Also update render worker layers (for deck.gl layers if still active)
    if (this.renderWorker) {
      this.renderWorker.postMessage({
        type: 'config',
        payload: {
          showMoistureLayer: this.toggleMoistureCheckbox.checked,
          showSOMLayer: this.toggleSOMCheckbox.checked,
          showVegetationLayer: this.toggleVegetationCheckbox.checked,
          showDifferenceMap: false, // Disable deck.gl difference layer (now using Cesium)
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

  /**
   * Initialize Cesium Primitive-based difference map
   * Replaces unstable deck.gl implementation with native Cesium geometries
   */
  private initDifferenceMap() {
    if (!this.viewer || !this.sab) {
      console.warn('[DifferenceMap] Cannot initialize - viewer or SAB not ready');
      return;
    }

    console.log('[DifferenceMap] Initializing Cesium primitives...');

    // Create primitive collection
    this.differenceCollection = new PrimitiveCollection();
    this.viewer.scene.primitives.add(this.differenceCollection);

    // Create geometry instances
    this.differenceInstances = this.createDifferenceGeometry();

    // Create primitive with per-instance color appearance
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

    console.log(`[DifferenceMap] ✓ Initialized with ${this.differenceInstances.length} cells`);
  }

  /**
   * Create geometry instances for each grid cell
   * Uses Rectangle geometry for perfect cell shapes at all angles
   */
  private createDifferenceGeometry(): GeometryInstance[] {
    const instances: GeometryInstance[] = [];
    const bbox = this.readBBoxFromSAB();

    const lonStep = (bbox.maxLon - bbox.minLon) / this.gridSize;
    const latStep = (bbox.maxLat - bbox.minLat) / this.gridSize;

    for (let row = 0; row < this.gridSize; row++) {
      for (let col = 0; col < this.gridSize; col++) {
        const lon = bbox.minLon + (col + 0.5) * lonStep;
        const lat = bbox.minLat + (row + 0.5) * latStep;

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
            color: ColorGeometryInstanceAttribute.fromColor(
              Color.WHITE.withAlpha(0) // Initially transparent
            )
          }
        });

        instances.push(instance);
      }
    }

    return instances;
  }

  /**
   * Update difference map colors based on current SOM vs baseline
   * Called each frame when difference map is visible
   */
  private updateDifferenceColors() {
    if (!this.somBaseline || !this.differenceInstances.length) return;

    const currentSOM = this.readSOMFromSAB();
    if (!currentSOM) return;

    // Update each instance's color attribute
    for (let i = 0; i < this.differenceInstances.length; i++) {
      const current = currentSOM[i];
      const baseline = this.somBaseline[i];
      const delta = current - baseline;

      // Normalize to [-1, +1] range (±30% max)
      const normalizedDelta = Math.max(-1, Math.min(1, delta / 0.3));

      // Get CliMA RdBu-11 color
      const color = this.getRdBu11Color(normalizedDelta);

      // Update instance color
      this.differenceInstances[i].attributes.color =
        ColorGeometryInstanceAttribute.fromColor(color);
    }

    // Recreate primitive to reflect color changes
    // Note: Cesium requires primitive recreation for attribute updates
    if (this.differenceCollection && this.differencePrimitive) {
      this.differenceCollection.remove(this.differencePrimitive);
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
    }
  }

  /**
   * CliMA RdBu-11 perceptual color palette for soil difference mapping
   * @param normalizedValue - Value in [-1, +1] range (-1=degradation, +1=restoration)
   * @returns Cesium Color with appropriate alpha
   */
  private getRdBu11Color(normalizedValue: number): Color {
    // Map [-1, +1] to [0, 1] for palette index
    const t = (normalizedValue + 1) / 2;

    // CliMA RdBu-11 palette (11 stops)
    const palette = [
      [165, 0, 38],      // 0.0 - Dark red (max degradation)
      [215, 48, 39],     // 0.1
      [244, 109, 67],    // 0.2
      [253, 174, 97],    // 0.3
      [254, 224, 144],   // 0.4
      [255, 255, 191],   // 0.5 - White (no change)
      [224, 243, 248],   // 0.6
      [171, 217, 233],   // 0.7
      [116, 173, 209],   // 0.8
      [69, 117, 180],    // 0.9
      [49, 54, 149],     // 1.0 - Dark blue (max restoration)
    ];

    // Interpolate between palette stops
    const index = t * 10;
    const i = Math.floor(index);
    const frac = index - i;
    const i1 = Math.min(i, 9);
    const i2 = Math.min(i + 1, 10);

    const c1 = palette[i1];
    const c2 = palette[i2];

    const r = c1[0] + (c2[0] - c1[0]) * frac;
    const g = c1[1] + (c2[1] - c1[1]) * frac;
    const b = c1[2] + (c2[2] - c1[2]) * frac;

    // Transparency: hide near-zero changes
    const alpha = Math.abs(normalizedValue) < 0.05 ? 0.0 : 0.78;

    return new Color(r / 255, g / 255, b / 255, alpha);
  }

  /**
   * Read SOM field from SharedArrayBuffer
   * @returns Float32Array of SOM values or null if not available
   */
  private readSOMFromSAB(): Float32Array | null {
    if (!this.sab) return null;

    const headerSize = 128;
    const gridSize = this.gridSize * this.gridSize;
    const somOffset = headerSize + gridSize * 4; // Second field (after theta)

    return new Float32Array(this.sab, somOffset, gridSize);
  }

  /**
   * Read bounding box from SharedArrayBuffer header
   * @returns BoundingBox or default Kansas region
   */
  private readBBoxFromSAB(): BoundingBox {
    if (!this.sab) {
      // Default to Kansas test region
      return { minLon: -95.1, minLat: 39.9, maxLon: -94.9, maxLat: 40.1 };
    }

    const view = new DataView(this.sab);
    // BBox is at offset 24 (after version, epoch, rows, cols, timestamp)
    return {
      minLon: view.getFloat32(24, true),
      minLat: view.getFloat32(28, true),
      maxLon: view.getFloat32(32, true),
      maxLat: view.getFloat32(36, true),
    };
  }

  /**
   * Capture current SOM state as baseline for difference mapping
   */
  private captureBaseline() {
    const somData = this.readSOMFromSAB();
    if (somData) {
      this.somBaseline = new Float32Array(somData);
      const meanSOM = somData.reduce((a, b) => a + b, 0) / somData.length;
      console.log(`[DifferenceMap] ✓ Baseline captured: ${somData.length} cells, mean SOM = ${meanSOM.toFixed(4)}`);
    } else {
      console.warn('[DifferenceMap] ✗ Cannot capture baseline - no SOM data');
    }
  }

  /**
   * Start difference map update loop
   * Runs continuously at ~60 FPS when difference map is visible
   */
  private startDifferenceMapLoop() {
    const update = () => {
      if (this.showDifferenceMap && this.somBaseline) {
        this.updateDifferenceColors();
      }
      requestAnimationFrame(update);
    };
    update();
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
