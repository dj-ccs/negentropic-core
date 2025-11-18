// --------------------------------------------------------------------
// web/src/geometry/clima-matrices.ts
// --------------------------------------------------------------------
// Extracted/Adapted from CliMA ClimaCore.jl v0.6.0 (Geometry/CubedSphere.jl)
// Faces: 0=+Z (north), 1=-Z (south), 2=+X (east), 3=-X (west), 4=+Y (front), 5=-Y (back)

export const CLIMA_FACE_MATRICES: Float32Array[] = [
  // Face 0: +Z (Identity - north pole)
  new Float32Array([
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  ]),
  // Face 1: -Z (Diag flip for south pole)
  new Float32Array([
    1.0, 0.0, 0.0, 0.0,
    0.0, -1.0, 0.0, 0.0,
    0.0, 0.0, -1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  ]),
  // Face 2: +X (90° rot around Y, then 90° around Z)
  new Float32Array([
    0.0, 0.0, 1.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    -1.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  ]),
  // Face 3: -X (Mirror of +X)
  new Float32Array([
    0.0, 0.0, -1.0, 0.0,
    0.0, -1.0, 0.0, 0.0,
    1.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  ]),
  // Face 4: +Y (90° rot around X, negated Z)
  new Float32Array([
    1.0, 0.0, 0.0, 0.0,
    0.0, 0.0, -1.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  ]),
  // Face 5: -Y (Mirror of +Y)
  new Float32Array([
    -1.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, -1.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  ])
];
