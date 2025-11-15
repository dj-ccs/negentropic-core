/**
 * GEO-v1 Main Entry Point
 * Thread 1 (Main): Orchestrates Cesium globe, creates SAB, spawns workers
 */

import { Viewer, Cartesian3, Color, ScreenSpaceEventHandler, ScreenSpaceEventType } from 'cesium';
import 'cesium/Build/Cesium/Widgets/widgets.css';
import type {
  GridSpec,
  BoundingBox,
  FieldOffsets,
  PerformanceMetrics
} from './types/geo-api';

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

    this.viewer = new Viewer(container, {
      timeline: false,
      animation: false,
      baseLayerPicker: true,
      geocoder: true,
      homeButton: true,
      sceneModePicker: true,
      navigationHelpButton: false,
      selectionIndicator: false,
      infoBox: false,
      requestRenderMode: false, // Continuous rendering for 60 FPS
      maximumRenderTimeChange: Infinity,
    });

    // Set initial camera position
    this.viewer.camera.setView({
      destination: Cartesian3.fromDegrees(-95.0, 40.0, 15000000.0),
    });

    // Add click handler for region selection
    this.setupGlobeClickHandler();

    console.log('✓ Cesium initialized');
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
          case 'ready':
            console.log('✓ Core Worker ready');
            // Transfer SAB to worker
            this.coreWorker!.postMessage({
              type: 'init',
              payload: { sab: this.sab },
            });
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

      // Timeout
      setTimeout(() => reject(new Error('Core Worker timeout')), 10000);
    });
  }

  private async spawnRenderWorker() {
    return new Promise<void>((resolve, reject) => {
      this.renderWorker = new Worker(new URL('./workers/render-worker.ts', import.meta.url), {
        type: 'module',
      });

      this.renderWorker.onmessage = (e) => {
        const { type, payload } = e.data;

        switch (type) {
          case 'ready':
            console.log('✓ Render Worker ready');

            // Transfer SAB and OffscreenCanvas to worker
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

      // Timeout
      setTimeout(() => reject(new Error('Render Worker timeout')), 10000);
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
