// --------------------------------------------------------------------
// web/src/geometry/get-face.ts
// --------------------------------------------------------------------
import { Cartographic } from 'cesium'; // Must be imported in main thread

/**
 * Determines CliMA cubed-sphere face from Cesium camera position.
 * @param cameraPosition Cartographic (lon, lat, height) or ECEF Cartesian3.
 * @returns Face index (0-5) as defined in CLIMA_FACE_MATRICES.
 */
export function getClimaCoreFace(cameraPosition: Cartographic | any): number {
  // Convert to ECEF if Cartographic
  let ecef: { x: number; y: number; z: number };
  if (cameraPosition.longitude !== undefined) {
    // NOTE: This ECEF conversion MUST happen via Cesium's utilities in the main thread
    ecef = Cartographic.toCartesian(cameraPosition);
  } else {
    ecef = cameraPosition;
  }

  const { x, y, z } = ecef;
  const absX = Math.abs(x), absY = Math.abs(y), absZ = Math.abs(z);
  let maxAbs = absX;
  let face = 2; // Default +X

  if (absY > maxAbs) {
    maxAbs = absY;
    face = y > 0 ? 4 : 5; // +Y or -Y
  }
  if (absZ > maxAbs) {
    maxAbs = absZ;
    face = z > 0 ? 0 : 1; // +Z or -Z
  } else if (absX > maxAbs) {
    face = x > 0 ? 2 : 3; // +X or -X
  }

  // Final check if maxAbs hasn't been updated (can happen near origin, use initial default)
  if (maxAbs === absX) {
      face = x > 0 ? 2 : 3; // Re-evaluate based on X, Y, Z
      if (absY > absX && absY > absZ) { face = y > 0 ? 4 : 5; }
      else if (absZ > absX && absZ > absY) { face = z > 0 ? 0 : 1; }
  }

  return face;
}
