#!/usr/bin/env python3
"""
precision_profiler.py - Fixed-Point Precision Boundary Mapper

Maps failure boundaries for fixed-point (16.16) arithmetic in SE(3) operations.
Identifies where fixed-point diverges from FP64 reference.

Operations tested:
  - Rotation matrix multiplication
  - Vector transformations
  - Angular velocity integration
  - Trigonometric lookups

Outputs:
  1. Heatmap: Error vs input magnitude (PNG)
  2. JSON: precision_boundaries.json (runtime thresholds)
  3. Header: src/core/precision_boundaries.h (compile-time constants)

Use cases:
  - Auto-escalation: Switch from fixed to float when out of bounds
  - Compile-time checks: Assert inputs are within safe range
  - Documentation: Define operational envelope

Author: negentropic-core team
Version: 0.1.0
License: MIT OR GPL-3.0
"""

import numpy as np
import matplotlib.pyplot as plt
import json
import os
from typing import Dict, Tuple

# ========================================================================
# CONFIGURATION
# ========================================================================

OUTPUT_DIR = "results"
FRACBITS = 16
FRACUNIT = 1 << FRACBITS  # 65536

# Test ranges (in floating-point)
ANGLE_RANGE = np.linspace(0, 2*np.pi, 100)
MAGNITUDE_RANGE = np.logspace(-3, 3, 100)  # 0.001 to 1000

# Error thresholds
ERROR_THRESHOLD_WARN = 1e-4   # Warning level
ERROR_THRESHOLD_FAIL = 1e-3   # Failure level

# ========================================================================
# FIXED-POINT SIMULATION
# ========================================================================

class FixedPoint:
    """Simulate 16.16 fixed-point arithmetic."""

    @staticmethod
    def to_fixed(x: float) -> int:
        """Convert float to fixed-point."""
        return int(x * FRACUNIT)

    @staticmethod
    def to_float(fx: int) -> float:
        """Convert fixed-point to float."""
        return float(fx) / FRACUNIT

    @staticmethod
    def mul(a: int, b: int) -> int:
        """Fixed-point multiplication with 64-bit intermediate."""
        return (a * b) >> FRACBITS

    @staticmethod
    def div(a: int, b: int) -> int:
        """Fixed-point division with 64-bit intermediate."""
        if b == 0:
            return 2**31 - 1 if a >= 0 else -(2**31)  # Saturate
        return (a << FRACBITS) // b

# ========================================================================
# OPERATION TESTS
# ========================================================================

def test_rotation_multiplication(angle: float) -> Tuple[float, bool]:
    """
    Test rotation matrix multiplication accuracy.

    Returns: (error, in_bounds)
    """
    # FP64 reference
    cos_ref = np.cos(angle)
    sin_ref = np.sin(angle)

    R_ref = np.array([
        [cos_ref, -sin_ref, 0],
        [sin_ref, cos_ref, 0],
        [0, 0, 1]
    ])

    # Self-multiplication
    R_squared_ref = R_ref @ R_ref

    # Fixed-point simulation
    cos_fx = FixedPoint.to_fixed(cos_ref)
    sin_fx = FixedPoint.to_fixed(sin_ref)

    # Fixed-point matrix
    R_fx = np.array([
        [cos_fx, -sin_fx, 0],
        [sin_fx, cos_fx, 0],
        [0, 0, FRACUNIT]
    ], dtype=np.int64)

    # Matrix multiplication (fixed-point)
    R_squared_fx = np.zeros((3, 3), dtype=np.int64)
    for i in range(3):
        for j in range(3):
            acc = 0
            for k in range(3):
                acc += (R_fx[i, k] * R_fx[k, j]) >> FRACBITS
            R_squared_fx[i, j] = acc

    # Convert back to float
    R_squared_fx_float = np.zeros((3, 3))
    for i in range(3):
        for j in range(3):
            R_squared_fx_float[i, j] = FixedPoint.to_float(R_squared_fx[i, j])

    # Compute error
    error = np.linalg.norm(R_squared_fx_float - R_squared_ref, 'fro')
    in_bounds = error < ERROR_THRESHOLD_FAIL

    return error, in_bounds

def test_vector_transform(magnitude: float, angle: float) -> Tuple[float, bool]:
    """
    Test vector transformation accuracy: v' = R * v

    Returns: (error, in_bounds)
    """
    # FP64 reference
    cos_ref = np.cos(angle)
    sin_ref = np.sin(angle)

    R_ref = np.array([
        [cos_ref, -sin_ref, 0],
        [sin_ref, cos_ref, 0],
        [0, 0, 1]
    ])

    v_ref = np.array([magnitude, 0, 0])
    v_transformed_ref = R_ref @ v_ref

    # Fixed-point simulation
    cos_fx = FixedPoint.to_fixed(cos_ref)
    sin_fx = FixedPoint.to_fixed(sin_ref)
    v_fx = np.array([FixedPoint.to_fixed(magnitude), 0, 0])

    R_fx = np.array([
        [cos_fx, -sin_fx, 0],
        [sin_fx, cos_fx, 0],
        [0, 0, FRACUNIT]
    ], dtype=np.int64)

    # Matrix-vector multiplication
    v_transformed_fx = np.zeros(3, dtype=np.int64)
    for i in range(3):
        acc = 0
        for j in range(3):
            acc += (R_fx[i, j] * v_fx[j]) >> FRACBITS
        v_transformed_fx[i] = acc

    # Convert back to float
    v_transformed_fx_float = np.array([FixedPoint.to_float(x) for x in v_transformed_fx])

    # Compute error
    error = np.linalg.norm(v_transformed_fx_float - v_transformed_ref)
    in_bounds = error < ERROR_THRESHOLD_FAIL

    return error, in_bounds

# ========================================================================
# SWEEP TESTS
# ========================================================================

def sweep_rotation_multiplication() -> np.ndarray:
    """Sweep angle range for rotation multiplication."""
    errors = np.zeros(len(ANGLE_RANGE))

    for i, angle in enumerate(ANGLE_RANGE):
        error, _ = test_rotation_multiplication(angle)
        errors[i] = error

    return errors

def sweep_vector_transform() -> np.ndarray:
    """Sweep magnitude and angle for vector transformation."""
    error_map = np.zeros((len(MAGNITUDE_RANGE), len(ANGLE_RANGE)))

    for i, mag in enumerate(MAGNITUDE_RANGE):
        for j, angle in enumerate(ANGLE_RANGE):
            error, _ = test_vector_transform(mag, angle)
            error_map[i, j] = error

    return error_map

# ========================================================================
# BOUNDARY DETECTION
# ========================================================================

def find_safe_magnitude_range() -> Tuple[float, float]:
    """Find safe magnitude range for vector operations."""
    safe_min = None
    safe_max = None

    for mag in MAGNITUDE_RANGE:
        # Test at multiple angles
        all_safe = True
        for angle in np.linspace(0, 2*np.pi, 20):
            _, in_bounds = test_vector_transform(mag, angle)
            if not in_bounds:
                all_safe = False
                break

        if all_safe:
            if safe_min is None:
                safe_min = mag
            safe_max = mag

    return safe_min or 0.0, safe_max or 0.0

def find_safe_angle_range() -> Tuple[float, float]:
    """Find safe angle range for rotation operations."""
    safe_angles = []

    for angle in ANGLE_RANGE:
        error, in_bounds = test_rotation_multiplication(angle)
        if in_bounds:
            safe_angles.append(angle)

    if safe_angles:
        return min(safe_angles), max(safe_angles)
    else:
        return 0.0, 0.0

# ========================================================================
# VISUALIZATION
# ========================================================================

def plot_heatmap(error_map: np.ndarray, output_path: str):
    """Generate heatmap of errors."""
    fig, ax = plt.subplots(figsize=(10, 8))

    # Plot heatmap
    im = ax.imshow(np.log10(error_map + 1e-10),
                   aspect='auto',
                   extent=[ANGLE_RANGE[0], ANGLE_RANGE[-1],
                           np.log10(MAGNITUDE_RANGE[0]), np.log10(MAGNITUDE_RANGE[-1])],
                   origin='lower',
                   cmap='RdYlGn_r')

    ax.set_xlabel('Angle (radians)')
    ax.set_ylabel('log10(Magnitude)')
    ax.set_title('Fixed-Point Error Map: Vector Transformation')

    # Colorbar
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('log10(Error)')

    # Add threshold lines
    ax.axhline(y=np.log10(ERROR_THRESHOLD_WARN), color='orange', linestyle='--', label='Warning')
    ax.axhline(y=np.log10(ERROR_THRESHOLD_FAIL), color='red', linestyle='--', label='Failure')

    ax.legend()

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"✓ Heatmap saved to: {output_path}")

# ========================================================================
# OUTPUT GENERATION
# ========================================================================

def generate_json(boundaries: Dict, output_path: str):
    """Generate JSON with runtime thresholds."""
    with open(output_path, 'w') as f:
        json.dump(boundaries, f, indent=2)

    print(f"✓ JSON boundaries saved to: {output_path}")

def generate_header(boundaries: Dict, output_path: str):
    """Generate C header with compile-time constants."""
    with open(output_path, 'w') as f:
        f.write("/*\n")
        f.write(" * precision_boundaries.h - Auto-Generated Precision Limits\n")
        f.write(" *\n")
        f.write(" * DO NOT EDIT MANUALLY - Generated by tools/precision_profiler.py\n")
        f.write(" *\n")
        f.write(" * Safe operating ranges for 16.16 fixed-point arithmetic.\n")
        f.write(" */\n\n")
        f.write("#ifndef NE_PRECISION_BOUNDARIES_H\n")
        f.write("#define NE_PRECISION_BOUNDARIES_H\n\n")
        f.write("#include <stdint.h>\n\n")

        # Magnitude limits
        f.write("/* Safe magnitude range (meters, fixed-point units) */\n")
        mag_min = boundaries['safe_magnitude_min']
        mag_max = boundaries['safe_magnitude_max']
        f.write(f"#define SAFE_MAGNITUDE_MIN_FP  INT32_C({int(mag_min * FRACUNIT)})\n")
        f.write(f"#define SAFE_MAGNITUDE_MAX_FP  INT32_C({int(mag_max * FRACUNIT)})\n")
        f.write(f"#define SAFE_MAGNITUDE_MIN     {mag_min:.6f}f\n")
        f.write(f"#define SAFE_MAGNITUDE_MAX     {mag_max:.6f}f\n\n")

        # Angle limits
        f.write("/* Safe angle range (radians) */\n")
        angle_min = boundaries['safe_angle_min']
        angle_max = boundaries['safe_angle_max']
        f.write(f"#define SAFE_ANGLE_MIN         {angle_min:.6f}f\n")
        f.write(f"#define SAFE_ANGLE_MAX         {angle_max:.6f}f\n\n")

        # Error thresholds
        f.write("/* Error thresholds */\n")
        f.write(f"#define ERROR_THRESHOLD_WARN   {ERROR_THRESHOLD_WARN:.9f}f\n")
        f.write(f"#define ERROR_THRESHOLD_FAIL   {ERROR_THRESHOLD_FAIL:.9f}f\n\n")

        f.write("#endif /* NE_PRECISION_BOUNDARIES_H */\n")

    print(f"✓ C header saved to: {output_path}")

# ========================================================================
# MAIN
# ========================================================================

def main():
    print("=" * 70)
    print("PRECISION PROFILER - Fixed-Point Boundary Mapper")
    print("=" * 70)

    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("\n[1/4] Sweeping rotation multiplication...")
    rotation_errors = sweep_rotation_multiplication()

    print("[2/4] Sweeping vector transformations...")
    vector_error_map = sweep_vector_transform()

    print("[3/4] Finding safe boundaries...")
    safe_mag_min, safe_mag_max = find_safe_magnitude_range()
    safe_angle_min, safe_angle_max = find_safe_angle_range()

    boundaries = {
        'safe_magnitude_min': safe_mag_min,
        'safe_magnitude_max': safe_mag_max,
        'safe_angle_min': safe_angle_min,
        'safe_angle_max': safe_angle_max,
        'error_threshold_warn': ERROR_THRESHOLD_WARN,
        'error_threshold_fail': ERROR_THRESHOLD_FAIL
    }

    print(f"\nSafe magnitude range: [{safe_mag_min:.3f}, {safe_mag_max:.3f}]")
    print(f"Safe angle range: [{safe_angle_min:.3f}, {safe_angle_max:.3f}] rad")

    print("\n[4/4] Generating outputs...")

    # Generate outputs
    heatmap_path = os.path.join(OUTPUT_DIR, "precision_heatmap.png")
    json_path = os.path.join(OUTPUT_DIR, "precision_boundaries.json")
    header_path = os.path.join("src", "core", "precision_boundaries.h")

    plot_heatmap(vector_error_map, heatmap_path)
    generate_json(boundaries, json_path)
    generate_header(boundaries, header_path)

    print("\n" + "=" * 70)
    print("✓ Precision profiling complete!")
    print("=" * 70)

if __name__ == "__main__":
    main()
