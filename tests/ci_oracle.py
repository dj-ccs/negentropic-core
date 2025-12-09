#!/usr/bin/env python3
"""
ci_oracle.py - Genesis v3.0 CI Oracle for Ensemble Validation

Implements the CI validation protocol per Genesis Principle #5:
"Domain Randomization is Calibration - every parameter is a distribution"

This script:
1. Runs 100 randomized ensemble members with different RNG seeds
2. Computes mean and std_dev of final SOM and vegetation
3. Fails if relative std_dev > 4% (threshold from spec)

Usage:
    python tests/ci_oracle.py --config config/parameters/genesis_crop_params.json --runs 100

Author: negentropic-core team
Version: 0.4.0-alpha-genesis
Date: 2025-12-09
License: MIT OR GPL-3.0
"""

import argparse
import json
import os
import subprocess
import sys
import numpy as np
from pathlib import Path


def load_config(config_path: str) -> dict:
    """Load parameter configuration from JSON file."""
    with open(config_path, 'r') as f:
        return json.load(f)


def run_simulation(config_path: str, seed: int, years: int = 20, output_file: str = None) -> dict:
    """
    Run a single simulation with specified seed.

    In production, this would call the negentropic-core binary.
    For the skeleton implementation, we simulate the output.

    Returns:
        dict with 'som' and 'vegetation' final values
    """
    # Construct command (production implementation)
    # cmd = [
    #     "./negcore_run",
    #     "--config", config_path,
    #     "--seed", str(seed),
    #     "--run-to-year", str(years),
    #     "--export", output_file or "som_value.json"
    # ]

    # Skeleton implementation: simulate realistic output with variation
    np.random.seed(seed)

    # Base values from Loess Plateau calibration
    base_som = 0.02  # 2% SOM after 20 years of restoration
    base_veg = 0.65  # 65% vegetation cover

    # Add variation based on parameter randomization
    # Variation should be <4% relative std_dev to pass CI
    som_variation = np.random.normal(0, 0.001)  # ~5% relative variation
    veg_variation = np.random.normal(0, 0.02)   # ~3% relative variation

    final_som = max(0.001, base_som + som_variation)
    final_veg = max(0.01, min(1.0, base_veg + veg_variation))

    return {
        'som': final_som,
        'vegetation': final_veg,
        'seed': seed
    }


def run_ensemble(config_path: str, runs: int = 100, years: int = 20, seed_base: int = 42) -> list:
    """
    Run ensemble of simulations with different RNG seeds.

    Args:
        config_path: Path to parameter configuration JSON
        runs: Number of ensemble members
        years: Simulated years per run
        seed_base: Base seed (each run uses seed_base + run_index)

    Returns:
        List of result dicts
    """
    results = []

    print(f"[CI Oracle] Running {runs} ensemble members...")
    print(f"[CI Oracle] Config: {config_path}")
    print(f"[CI Oracle] Years: {years}, Seed base: {seed_base}")

    for i in range(runs):
        seed = seed_base + i
        result = run_simulation(config_path, seed, years)
        results.append(result)

        # Progress indicator
        if (i + 1) % 10 == 0:
            print(f"[CI Oracle] Completed {i + 1}/{runs} runs")

    return results


def compute_statistics(values: list) -> dict:
    """
    Compute mean, std_dev, and relative std_dev for a list of values.

    Returns:
        dict with 'mean', 'std', 'rel_std'
    """
    arr = np.array(values)
    mean = np.mean(arr)
    std = np.std(arr, ddof=1)  # Sample std dev
    rel_std = std / mean if mean > 0 else 0

    return {
        'mean': float(mean),
        'std': float(std),
        'rel_std': float(rel_std)
    }


def check_threshold(rel_std: float, threshold: float = 0.04) -> bool:
    """Check if relative std_dev is within threshold."""
    return rel_std <= threshold


def print_results(som_stats: dict, veg_stats: dict, threshold: float = 0.04):
    """Print formatted results summary."""
    print("\n" + "=" * 70)
    print("GENESIS v3.0 CI ORACLE - ENSEMBLE VALIDATION RESULTS")
    print("=" * 70)

    print(f"\nSOM (Soil Organic Matter):")
    print(f"  Mean:         {som_stats['mean']:.6f} ({som_stats['mean']*100:.2f}%)")
    print(f"  Std Dev:      {som_stats['std']:.6f}")
    print(f"  Relative Std: {som_stats['rel_std']*100:.2f}%")
    print(f"  Status:       {'PASS' if check_threshold(som_stats['rel_std'], threshold) else 'FAIL'}")

    print(f"\nVegetation Cover:")
    print(f"  Mean:         {veg_stats['mean']:.6f} ({veg_stats['mean']*100:.2f}%)")
    print(f"  Std Dev:      {veg_stats['std']:.6f}")
    print(f"  Relative Std: {veg_stats['rel_std']*100:.2f}%")
    print(f"  Status:       {'PASS' if check_threshold(veg_stats['rel_std'], threshold) else 'FAIL'}")

    print(f"\nThreshold: {threshold*100:.1f}% relative std_dev")
    print("=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="Genesis v3.0 CI Oracle for ensemble validation",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tests/ci_oracle.py --config config/parameters/genesis_crop_params.json
  python tests/ci_oracle.py --runs 100 --years 20 --threshold 0.04
        """
    )

    parser.add_argument(
        '--config',
        type=str,
        default='config/parameters/genesis_crop_params.json',
        help='Path to parameter configuration JSON'
    )
    parser.add_argument(
        '--runs',
        type=int,
        default=100,
        help='Number of ensemble runs (default: 100)'
    )
    parser.add_argument(
        '--years',
        type=int,
        default=20,
        help='Simulated years per run (default: 20)'
    )
    parser.add_argument(
        '--seed-base',
        type=int,
        default=42,
        help='Base RNG seed (default: 42)'
    )
    parser.add_argument(
        '--threshold',
        type=float,
        default=0.04,
        help='Maximum relative std_dev threshold (default: 0.04 = 4%%)'
    )
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Print detailed output'
    )

    args = parser.parse_args()

    # Check config file exists (warning only for skeleton)
    if not os.path.exists(args.config):
        print(f"[Warning] Config file not found: {args.config}")
        print("[Warning] Using default parameters for skeleton test")

    # Run ensemble
    results = run_ensemble(
        args.config,
        runs=args.runs,
        years=args.years,
        seed_base=args.seed_base
    )

    # Extract SOM and vegetation values
    som_values = [r['som'] for r in results]
    veg_values = [r['vegetation'] for r in results]

    # Compute statistics
    som_stats = compute_statistics(som_values)
    veg_stats = compute_statistics(veg_values)

    # Print results
    print_results(som_stats, veg_stats, args.threshold)

    # Determine pass/fail
    som_pass = check_threshold(som_stats['rel_std'], args.threshold)
    veg_pass = check_threshold(veg_stats['rel_std'], args.threshold)

    if som_pass and veg_pass:
        print("\n[CI Oracle] OVERALL: PASS")
        print("[CI Oracle] Ensemble variance within acceptable bounds")
        print("[CI Oracle] Domain randomization calibration validated")
        return 0
    else:
        print("\n[CI Oracle] OVERALL: FAIL")
        if not som_pass:
            print(f"[CI Oracle] SOM relative std ({som_stats['rel_std']*100:.2f}%) exceeds threshold ({args.threshold*100:.1f}%)")
        if not veg_pass:
            print(f"[CI Oracle] Vegetation relative std ({veg_stats['rel_std']*100:.2f}%) exceeds threshold ({args.threshold*100:.1f}%)")
        return 1


if __name__ == '__main__':
    sys.exit(main())
