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
  UrlTemplateImageryProvider
} from 'cesium';
import 'cesium/Build/Cesium/Widgets/widgets.css';
import type {
  GridSpec,
  BoundingBox,
  FieldOffsets,
  PerformanceMetrics
} from './types/geo-api';

// Configure Cesium Ion token
const CESIUM_ION_TOKEN = import.meta.env.VITE_CESIUM_ION_TOKEN;
if (CESIUM_ION_TOKEN && CESIUM_ION_TOKEN !== 'your_token_here') {
  Ion.defaultAccessToken = CESIUM_ION_TOKEN;
  console.log('✓ Cesium Ion token configured');
} else {
  console.warn('⚠ No Cesium Ion token configured. Using default token (limited features)');
  console.warn('  Get your free token at: https://cesium.com/ion/tokens');
  console.warn('  Add it to web/.env as: VITE_CESIUM_ION_TOKEN=your_token');
}

// Import worker URLs (Vite will handle bundling)
import CoreWorkerURL from './workers/core-worker?worker&url';
import RenderWorkerURL from './workers/render-worker?worker&url';

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

  // UI Elements
  private loadingScreen: HTMLElement;
  private loadingStatus: HTMLElement;
  private systemStatus: HTMLElement;
  private coreFpsDisplay: HTMLElement;
  private renderFpsDisplay: HTMLElement;
  private activeRegionDisplay: HTMLElement;
  private startBtn: HTMLButtonElement;
  private pauseBtn: HTMLButtonElement;

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

    // Bind event handlers
    this.startBtn.addEventListener('click', () => this.startSimulation());
    this.pauseBtn.addEventListener('click', () => this.pauseSimulation());
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

    console.log('Creating Cesium Viewer...');
    console.log('Ion token status:', Ion.defaultAccessToken ? 'Configured' : 'Using default');

    try {
      // Create OSM imagery provider BEFORE viewer creation
      // CRITICAL: Pass to Viewer constructor (docs/CESIUM_GUIDE.md line 103-107)
      // Using UrlTemplateImageryProvider with proxy URL (OpenStreetMapImageryProvider doesn't support custom URLs)
      console.log('Creating OpenStreetMap imagery provider...');
      const osmProvider = new UrlTemplateImageryProvider({
        // Use proxied URL to bypass COEP blocking (Vite proxy adds CORP headers)
        url: '/osm-tiles/{z}/{x}/{y}.png', // OSM tile URL pattern via Vite proxy
        credit: '© OpenStreetMap contributors'
      });

      // Create viewer with OSM as base imagery (see docs/CESIUM_GUIDE.md line 26-68)
      // CRITICAL: MUST explicitly set baseLayer: true for imagery to be added!
      console.log('Creating Cesium Viewer with OSM imagery...');
      this.viewer = new Viewer(container, {
        imageryProvider: osmProvider, // THE FIX: Pass to constructor (CESIUM_GUIDE.md line 433)
        baseLayerPicker: false, // Disable UI picker, but KEEP base layer functionality
        baseLayer: true, // CRITICAL: Explicitly enable base layer (Grok's INTERCONNECTION_GUIDE)
        timeline: false,
        animation: false,
        geocoder: true,
        homeButton: true,
        sceneModePicker: true,
        navigationHelpButton: false,
        selectionIndicator: false,
        infoBox: false,
        requestRenderMode: false, // Continuous rendering for 60 FPS
        maximumRenderTimeChange: Infinity,
      });

      // Expose viewer globally for debugging
      // @ts-ignore
      window.viewer = this.viewer;

      // ==========================================
      // GLOBE VISIBILITY HARDENING (Grok's fix)
      // ==========================================
      // Force globe visibility - this is critical for imagery to render
      this.viewer.scene.globe.show = true;
      this.viewer.scene.skyBox.show = true;
      this.viewer.scene.backgroundColor = Color.BLACK;

      // Optional: Additional globe configuration for better visibility
      this.viewer.scene.globe.baseColor = Color.fromCssColorString('#2e4057');
      this.viewer.scene.globe.enableLighting = false;
      this.viewer.scene.globe.showGroundAtmosphere = false;

      // Force render on first frame to ensure globe appears
      this.viewer.scene.render();

      // Disable sun/moon for simpler visualization
      if (this.viewer.scene.sun) this.viewer.scene.sun.show = false;
      if (this.viewer.scene.moon) this.viewer.scene.moon.show = false;

      // Log viewer creation success
      console.log('✓ Cesium Viewer created');
      console.log('  - Scene mode:', this.viewer.scene.mode);
      console.log('  - Globe enabled:', this.viewer.scene.globe ? 'Yes' : 'No');
      console.log('  - Terrain provider:', this.viewer.terrainProvider?.constructor?.name || 'None');
      console.log('  - Imagery layers:', this.viewer.imageryLayers.length);

      // CRITICAL CHECK: Verify imagery layers were added (see docs/CESIUM_GUIDE.md)
      if (this.viewer.imageryLayers.length === 0) {
        console.error('❌ CRITICAL: No imagery layers! Globe will be invisible.');
        console.error('   Fix: Ensure imageryProvider is passed to Viewer constructor.');
        throw new Error('Imagery layers = 0 - globe initialization failed');
      }

      // Check if globe is rendering
      if (!this.viewer.scene.globe) {
        console.warn('⚠ Globe is not enabled in the scene!');
      }

      // Debug: Check imagery layer state
      if (this.viewer.imageryLayers.length > 0) {
        const imageryLayer = this.viewer.imageryLayers.get(0);
        console.log('[DEBUG] Imagery layer details:', {
          show: imageryLayer.show,
          alpha: imageryLayer.alpha,
          brightness: imageryLayer.brightness,
          contrast: imageryLayer.contrast,
          ready: imageryLayer.ready,
          providerReady: osmProvider.ready,
          providerType: osmProvider.constructor?.name || 'Unknown',
        });
      }

      // Add error handler for imagery loading failures
      osmProvider.errorEvent.addEventListener((error: any) => {
        console.error('❌ OSM Imagery provider error:', error);
      });

      // Monitor when imagery becomes ready
      if (!osmProvider.ready) {
        console.log('⏳ Waiting for OSM imagery provider to become ready...');
        const checkReady = () => {
          if (osmProvider.ready) {
            console.log('✓ OSM imagery provider is now ready!');
          } else {
            setTimeout(checkReady, 100);
          }
        };
        checkReady();
      } else {
        console.log('✓ OSM imagery provider is already ready');
      }

      // Set initial camera position
      console.log('Setting initial camera view...');
      this.viewer.camera.setView({
        destination: Cartesian3.fromDegrees(-95.0, 40.0, 15000000.0),
      });
      console.log('✓ Camera positioned');

      // Add click handler for region selection
      this.setupGlobeClickHandler();

      // Wait a frame to ensure scene is ready
      await new Promise(resolve => setTimeout(resolve, 100));

      // Debug: Check if globe is actually visible
      console.log('✓ Cesium initialized successfully');
      console.log('[DEBUG] Globe visibility check:', {
        globeShow: this.viewer.scene.globe.show,
        backgroundColor: this.viewer.scene.backgroundColor,
        canvasWidth: this.viewer.canvas.width,
        canvasHeight: this.viewer.canvas.height,
        canvasStyle: this.viewer.canvas.style.cssText,
        containerStyle: container.style.cssText,
      });

      // Debug: Check if Cesium canvas is actually in the DOM and visible
      const cesiumCanvas = this.viewer.canvas;
      const computedStyle = window.getComputedStyle(cesiumCanvas);
      console.log('[DEBUG] Cesium canvas computed styles:', {
        display: computedStyle.display,
        visibility: computedStyle.visibility,
        opacity: computedStyle.opacity,
        zIndex: computedStyle.zIndex,
        width: computedStyle.width,
        height: computedStyle.height,
      });

    } catch (error) {
      console.error('Failed to create Cesium Viewer:', error);
      throw error;
    }
  }

  private setupGlobeClickHandler() {
    if (!this.viewer) return;

    const handler = new ScreenSpaceEventHandler(this.viewer.scene.canvas);

    handler.setInputAction((click: any) => {
      const cartesian = this.viewer!.camera.pickEllipsoid(
        click.position,
        this.viewer!.scene.globe.ellipsoid
      );

      if (cartesian) {
        const cartographic = this.viewer!.scene.globe.ellipsoid.cartesianToCartographic(cartesian);
        const longitude = cartographic.longitude * (180 / Math.PI);
        const latitude = cartographic.latitude * (180 / Math.PI);

        this.onRegionSelected(longitude, latitude);
      }
    }, ScreenSpaceEventType.LEFT_CLICK);
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

    console.log('Simulation started');
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
