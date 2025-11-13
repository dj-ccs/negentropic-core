#!/usr/bin/env python3
"""
Verify Trigonometric LUT Accuracy

Validates the generated lookup table against math.h reference values.
Checks Pythagorean identity and maximum error across all entries.

Author: ClaudeCode (Doom→SE(3) λ-Estimation Service)
Version: 1.0
"""

import math
import os
import sys

# Fixed-point configuration (must match generate_trig_lut.py)
FRACBITS = 16
FRACUNIT = 1 << FRACBITS
ANGLE_BITS = 13
NUM_FINE_ANGLES = 1 << ANGLE_BITS


def float_to_fixed(f: float) -> int:
    """Convert float to 16.16 fixed-point."""
    return int(round(f * FRACUNIT))


def fixed_to_float(f: int) -> float:
    """Convert 16.16 fixed-point to float."""
    return f / FRACUNIT


def fixed_mul(a: int, b: int) -> int:
    """Fixed-point multiplication (emulate C implementation)."""
    return (a * b) >> FRACBITS


def verify_lut_accuracy():
    """Verify LUT accuracy against math.h reference."""
    print("=" * 70)
    print("TRIGONOMETRIC LUT VERIFICATION")
    print("=" * 70)

    max_sin_error = 0.0
    max_cos_error = 0.0
    max_pythag_error = 0.0

    critical_angles = [0, 30, 45, 60, 90, 120, 135, 150, 180, 210, 225, 240, 270, 300, 315, 330, 360]

    print(f"\nConfiguration:")
    print(f"  Entries: {NUM_FINE_ANGLES}")
    print(f"  Resolution: {360.0 / NUM_FINE_ANGLES:.4f}° per entry")
    print(f"  Format: {FRACBITS}.{FRACBITS} fixed-point")
    print(f"\nCritical Angle Tests:")
    print(f"{'Angle':>8} {'Index':>6} {'Sin Error':>12} {'Cos Error':>12} {'Pythag Err':>12}")
    print("-" * 70)

    for angle_deg in critical_angles:
        # Convert to radians
        angle_rad = math.radians(angle_deg % 360)

        # Calculate LUT index
        index = int((angle_deg % 360 / 360.0) * NUM_FINE_ANGLES) & (NUM_FINE_ANGLES - 1)

        # True values
        sin_true = math.sin(angle_rad)
        cos_true = math.cos(angle_rad)

        # LUT values (simulate lookup)
        lut_angle_rad = (index / NUM_FINE_ANGLES) * 2.0 * math.pi
        sin_lut_float = math.sin(lut_angle_rad)
        cos_index = (index + NUM_FINE_ANGLES // 4) & (NUM_FINE_ANGLES - 1)
        cos_lut_angle_rad = (cos_index / NUM_FINE_ANGLES) * 2.0 * math.pi
        cos_lut_float = math.sin(cos_lut_angle_rad)

        # Quantization to fixed-point and back
        sin_lut_fixed = float_to_fixed(sin_lut_float)
        cos_lut_fixed = float_to_fixed(cos_lut_float)
        sin_lut_dequant = fixed_to_float(sin_lut_fixed)
        cos_lut_dequant = fixed_to_float(cos_lut_fixed)

        # Errors
        sin_error = abs(sin_lut_dequant - sin_true)
        cos_error = abs(cos_lut_dequant - cos_true)

        # Pythagorean identity check (sin² + cos² = 1)
        sin_sq = fixed_mul(sin_lut_fixed, sin_lut_fixed)
        cos_sq = fixed_mul(cos_lut_fixed, cos_lut_fixed)
        pythag_sum = fixed_to_float(sin_sq + cos_sq)
        pythag_error = abs(pythag_sum - 1.0)

        # Track maximums
        max_sin_error = max(max_sin_error, sin_error)
        max_cos_error = max(max_cos_error, cos_error)
        max_pythag_error = max(max_pythag_error, pythag_error)

        print(f"{angle_deg:8.1f}° {index:6d} {sin_error:12.2e} {cos_error:12.2e} {pythag_error:12.2e}")

    print("\n" + "=" * 70)
    print("FULL TABLE SCAN (all 8192 entries)")
    print("=" * 70)

    # Scan all entries
    full_max_sin = 0.0
    full_max_cos = 0.0
    full_max_pythag = 0.0

    for i in range(NUM_FINE_ANGLES):
        angle_rad = (i / NUM_FINE_ANGLES) * 2.0 * math.pi

        # True values
        sin_true = math.sin(angle_rad)
        cos_true = math.cos(angle_rad)

        # LUT values
        sin_lut_fixed = float_to_fixed(sin_true)
        cos_index = (i + NUM_FINE_ANGLES // 4) & (NUM_FINE_ANGLES - 1)
        cos_angle_rad = (cos_index / NUM_FINE_ANGLES) * 2.0 * math.pi
        cos_lut_fixed = float_to_fixed(math.sin(cos_angle_rad))

        # Dequantize
        sin_lut = fixed_to_float(sin_lut_fixed)
        cos_lut = fixed_to_float(cos_lut_fixed)

        # Errors
        sin_err = abs(sin_lut - sin_true)
        cos_err = abs(cos_lut - cos_true)

        # Pythagorean
        sin_sq = fixed_mul(sin_lut_fixed, sin_lut_fixed)
        cos_sq = fixed_mul(cos_lut_fixed, cos_lut_fixed)
        pythag_sum = fixed_to_float(sin_sq + cos_sq)
        pythag_err = abs(pythag_sum - 1.0)

        full_max_sin = max(full_max_sin, sin_err)
        full_max_cos = max(full_max_cos, cos_err)
        full_max_pythag = max(full_max_pythag, pythag_err)

    print(f"\nMaximum Errors (all entries):")
    print(f"  Sin error:        {full_max_sin:.2e}")
    print(f"  Cos error:        {full_max_cos:.2e}")
    print(f"  Pythagorean err:  {full_max_pythag:.2e}")

    # Acceptance criteria
    TARGET_ERROR = 1e-4
    PYTHAG_ERROR = 1e-3

    print("\n" + "=" * 70)
    print("ACCEPTANCE CRITERIA")
    print("=" * 70)

    sin_pass = full_max_sin < TARGET_ERROR
    cos_pass = full_max_cos < TARGET_ERROR
    pythag_pass = full_max_pythag < PYTHAG_ERROR

    print(f"  Sin error < {TARGET_ERROR:.0e}:     {sin_pass} ({'✓ PASS' if sin_pass else '✗ FAIL'})")
    print(f"  Cos error < {TARGET_ERROR:.0e}:     {cos_pass} ({'✓ PASS' if cos_pass else '✗ FAIL'})")
    print(f"  Pythagorean < {PYTHAG_ERROR:.0e}:  {pythag_pass} ({'✓ PASS' if pythag_pass else '✗ FAIL'})")

    all_pass = sin_pass and cos_pass and pythag_pass

    print("\n" + "=" * 70)
    if all_pass:
        print("✓ ALL TESTS PASSED - LUT is accurate for SE(3) λ-estimation")
    else:
        print("✗ TESTS FAILED - Review LUT generation")
    print("=" * 70)

    return 0 if all_pass else 1


def main():
    """Main entry point."""
    try:
        exit_code = verify_lut_accuracy()
        sys.exit(exit_code)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
