/**
 * variance_overlay.ts - Genesis v3.0 Ensemble Variance Visualization
 *
 * Implements visualization of per-pixel standard deviation from ensemble runs.
 * Uses CliMA RdBu-11 compatible diverging colormap (blue-white-red).
 *
 * Genesis Principle #5: Domain Randomization is Calibration
 * Genesis Principle #6: Parallel Environments are the Unit of Scale
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

/**
 * CliMA RdBu-11 compatible diverging colormap.
 * Blue (low variance) -> White (mid) -> Red (high variance)
 *
 * This colormap is perceptually uniform and colorblind-safe.
 */
const CLIMA_RDBU_11 = [
  [5, 48, 97],      // 0.0 - Deep blue (very low variance)
  [33, 102, 172],   // 0.1
  [67, 147, 195],   // 0.2
  [146, 197, 222],  // 0.3
  [209, 229, 240],  // 0.4
  [247, 247, 247],  // 0.5 - White (neutral)
  [253, 219, 199],  // 0.6
  [244, 165, 130],  // 0.7
  [214, 96, 77],    // 0.8
  [178, 24, 43],    // 0.9
  [103, 0, 31]      // 1.0 - Deep red (very high variance)
];

/**
 * Interpolate color from CliMA RdBu-11 colormap.
 *
 * @param t Normalized value in [0, 1]
 * @returns RGB color array [r, g, b] with values in [0, 255]
 */
function interpolateClimaRdBu(t: number): [number, number, number] {
  // Clamp t to [0, 1]
  t = Math.max(0, Math.min(1, t));

  // Map to colormap index
  const idx = t * (CLIMA_RDBU_11.length - 1);
  const lo = Math.floor(idx);
  const hi = Math.min(lo + 1, CLIMA_RDBU_11.length - 1);
  const frac = idx - lo;

  // Linear interpolation between adjacent colors
  const c0 = CLIMA_RDBU_11[lo];
  const c1 = CLIMA_RDBU_11[hi];

  return [
    Math.round(c0[0] * (1 - frac) + c1[0] * frac),
    Math.round(c0[1] * (1 - frac) + c1[1] * frac),
    Math.round(c0[2] * (1 - frac) + c1[2] * frac)
  ];
}

/**
 * Simple blue-red diverging colormap (fallback).
 *
 * @param t Normalized value in [0, 1] where 0.5 is white
 * @returns RGB color array
 */
function simpleBlueRedColormap(t: number): [number, number, number] {
  t = Math.max(0, Math.min(1, t));

  if (t < 0.5) {
    // Blue side: interpolate blue -> white
    const s = t * 2;  // 0 to 1
    return [
      Math.round(255 * s),
      Math.round(255 * s),
      255
    ];
  } else {
    // Red side: interpolate white -> red
    const s = (t - 0.5) * 2;  // 0 to 1
    return [
      255,
      Math.round(255 * (1 - s)),
      Math.round(255 * (1 - s))
    ];
  }
}

/**
 * VarianceOverlay - Renders per-pixel standard deviation of ΔSOM from ensemble runs.
 *
 * Usage:
 *   const overlay = new VarianceOverlay(canvas);
 *   overlay.update(stdDevArray, width, height);
 */
export class VarianceOverlay {
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D | null;
  private useClimaColormap: boolean;

  /**
   * Create a variance overlay renderer.
   *
   * @param canvas Canvas element to render to
   * @param useClimaColormap Use CliMA RdBu-11 colormap (default: true)
   */
  constructor(canvas: HTMLCanvasElement, useClimaColormap: boolean = true) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.useClimaColormap = useClimaColormap;
  }

  /**
   * Update the overlay with new variance data.
   *
   * @param stdDevArray Float32Array of per-pixel standard deviations
   * @param width Grid width in pixels
   * @param height Grid height in pixels
   * @param maxStdDev Maximum expected std dev for normalization (default: auto)
   * @param opacity Overlay opacity (0-255, default: 180)
   */
  update(
    stdDevArray: Float32Array,
    width: number,
    height: number,
    maxStdDev: number = 0,
    opacity: number = 180
  ): void {
    if (!this.ctx) {
      console.error('VarianceOverlay: Canvas context not available');
      return;
    }

    // Resize canvas if needed
    if (this.canvas.width !== width || this.canvas.height !== height) {
      this.canvas.width = width;
      this.canvas.height = height;
    }

    // Auto-compute max std dev if not provided
    if (maxStdDev <= 0) {
      maxStdDev = 0;
      for (let i = 0; i < stdDevArray.length; i++) {
        if (stdDevArray[i] > maxStdDev) {
          maxStdDev = stdDevArray[i];
        }
      }
      if (maxStdDev <= 0) maxStdDev = 1;  // Prevent division by zero
    }

    // Create image data
    const imageData = this.ctx.createImageData(width, height);
    const data = imageData.data;

    // Colormap function
    const colormap = this.useClimaColormap ? interpolateClimaRdBu : simpleBlueRedColormap;

    // Fill image data
    for (let i = 0; i < width * height; i++) {
      const stdDev = stdDevArray[i] || 0;
      const normalized = stdDev / maxStdDev;  // 0 = low variance, 1 = high variance

      const [r, g, b] = colormap(normalized);

      const idx = i * 4;
      data[idx + 0] = r;
      data[idx + 1] = g;
      data[idx + 2] = b;
      data[idx + 3] = opacity;  // Semi-transparent
    }

    // Draw to canvas
    this.ctx.putImageData(imageData, 0, 0);
  }

  /**
   * Clear the overlay.
   */
  clear(): void {
    if (this.ctx) {
      this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    }
  }

  /**
   * Draw a legend for the colormap.
   *
   * @param legendCanvas Canvas for legend (separate from main overlay)
   * @param minLabel Label for minimum value
   * @param maxLabel Label for maximum value
   */
  drawLegend(
    legendCanvas: HTMLCanvasElement,
    minLabel: string = 'Low σ',
    maxLabel: string = 'High σ'
  ): void {
    const ctx = legendCanvas.getContext('2d');
    if (!ctx) return;

    const width = legendCanvas.width;
    const height = legendCanvas.height;
    const barHeight = 20;
    const margin = 5;

    // Clear
    ctx.clearRect(0, 0, width, height);

    // Draw colorbar
    const colormap = this.useClimaColormap ? interpolateClimaRdBu : simpleBlueRedColormap;
    for (let x = 0; x < width - 2 * margin; x++) {
      const t = x / (width - 2 * margin - 1);
      const [r, g, b] = colormap(t);
      ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
      ctx.fillRect(margin + x, margin, 1, barHeight);
    }

    // Draw border
    ctx.strokeStyle = '#333';
    ctx.strokeRect(margin, margin, width - 2 * margin, barHeight);

    // Draw labels
    ctx.fillStyle = '#333';
    ctx.font = '12px sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(minLabel, margin, margin + barHeight + 15);
    ctx.textAlign = 'right';
    ctx.fillText(maxLabel, width - margin, margin + barHeight + 15);
  }
}

/**
 * Standalone function for quick variance overlay rendering.
 * For use without instantiating the class.
 *
 * @param canvas Target canvas
 * @param sdArray Standard deviation array (normalized to [0, 1])
 * @param width Grid width
 * @param height Grid height
 */
export function drawVarianceOverlay(
  canvas: HTMLCanvasElement,
  sdArray: Float32Array,
  width: number,
  height: number
): void {
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    console.error('drawVarianceOverlay: Canvas context not available');
    return;
  }

  // Resize canvas
  canvas.width = width;
  canvas.height = height;

  const imageData = ctx.createImageData(width, height);
  const data = imageData.data;

  for (let i = 0; i < width * height; i++) {
    const sd = sdArray[i] || 0;
    const t = Math.max(0, Math.min(1, sd));  // Assume pre-normalized

    // Simple diverging colormap
    const r = Math.floor(255 * t);
    const b = Math.floor(255 * (1 - t));
    const g = Math.floor(255 * (1 - Math.abs(2 * t - 1)));

    const idx = i * 4;
    data[idx + 0] = r;
    data[idx + 1] = g;
    data[idx + 2] = b;
    data[idx + 3] = 180;  // Semi-transparent
  }

  ctx.putImageData(imageData, 0, 0);
}

/**
 * Compute per-pixel standard deviation from ensemble results.
 *
 * @param ensembleData Array of 2D arrays, one per ensemble member
 * @param width Grid width
 * @param height Grid height
 * @returns Float32Array of standard deviations
 */
export function computeEnsembleStdDev(
  ensembleData: Float32Array[],
  width: number,
  height: number
): Float32Array {
  const numRuns = ensembleData.length;
  const numCells = width * height;
  const stdDevs = new Float32Array(numCells);

  if (numRuns === 0) return stdDevs;

  for (let i = 0; i < numCells; i++) {
    // Compute mean
    let sum = 0;
    for (let r = 0; r < numRuns; r++) {
      sum += ensembleData[r][i] || 0;
    }
    const mean = sum / numRuns;

    // Compute variance
    let sumSq = 0;
    for (let r = 0; r < numRuns; r++) {
      const diff = (ensembleData[r][i] || 0) - mean;
      sumSq += diff * diff;
    }
    const variance = numRuns > 1 ? sumSq / (numRuns - 1) : 0;

    stdDevs[i] = Math.sqrt(variance);
  }

  return stdDevs;
}
