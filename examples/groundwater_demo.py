#!/usr/bin/env python3
"""
Groundwater Flow Demo

Demonstrates SE(3)-based hydrological simulation with groundwater flow,
water table dynamics, and surface-subsurface interactions.

Status: Demo using stub implementation (v0.1.0-alpha)
"""

import numpy as np
import sys
from pathlib import Path

# Add src to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from physics import HydrologySolver
from simulation import SimulationKernel, SimulationConfig


def create_initial_conditions_recharge():
    """Create initial conditions with recharge zone."""
    nx, ny, nz = 32, 32, 8

    # Water table with elevation gradient (higher in center)
    x = np.linspace(-1, 1, nx)
    y = np.linspace(-1, 1, ny)
    X, Y = np.meshgrid(x, y, indexing='ij')

    # Elevated water table in center (recharge mound)
    water_table_depth = 10.0 - 5.0 * np.exp(-(X**2 + Y**2) / 0.3)

    # Soil moisture (30% baseline, higher near water table)
    soil_moisture = np.ones((nx, ny, nz)) * 0.3
    soil_moisture[:, :, -1] = 0.8  # Saturated near water table

    # No surface water initially
    surface_water = np.zeros((nx, ny))

    # No initial flow
    groundwater_flow = np.zeros((nx, ny, nz, 3))

    return {
        "hydrology": {
            "water_table_depth": water_table_depth,
            "soil_moisture": soil_moisture,
            "surface_water": surface_water,
            "groundwater_flow": groundwater_flow,
            "timestamp": 0.0
        }
    }


def run_groundwater_demo():
    """Run groundwater flow demonstration."""
    print("=" * 70)
    print("NEGENTROPIC-CORE: Groundwater Flow Demo")
    print("=" * 70)
    print()
    print("This demo simulates groundwater flow and water table dynamics")
    print("using a stub SE(3)-based solver (to be fully implemented).")
    print()

    # Configure simulation
    config = SimulationConfig(
        grid_size=(32, 32, 8),
        dt=3600.0,  # 1 hour time steps
        max_steps=48,  # 48 hours
        output_interval=6,  # Output every 6 hours
        enable_atmosphere=False,
        enable_hydrology=True,
        enable_soil=False
    )

    # Create kernel
    kernel = SimulationKernel(config)

    # Initialize with recharge mound
    initial_conditions = create_initial_conditions_recharge()
    kernel.initialize(initial_conditions)

    # Get initial diagnostics
    print("Initial conditions:")
    initial_diag = kernel.compute_diagnostics()
    print(f"  Mean water table depth: {initial_diag['hydrology']['mean_water_table_depth']:.2f} m")
    print(f"  Mean soil moisture: {initial_diag['hydrology']['mean_soil_moisture']:.3f}")
    print(f"  Total surface water: {initial_diag['hydrology']['total_surface_water']:.2f} m³")
    print(f"  Max groundwater flow: {initial_diag['hydrology']['max_groundwater_flow']:.4f} m/s")
    print()

    # Run simulation
    results = kernel.run(save_history=True)

    # Get final diagnostics
    print("\nFinal state (after 48 hours):")
    final_diag = results['diagnostics']
    print(f"  Mean water table depth: {final_diag['hydrology']['mean_water_table_depth']:.2f} m")
    print(f"  Mean soil moisture: {final_diag['hydrology']['mean_soil_moisture']:.3f}")
    print(f"  Total surface water: {final_diag['hydrology']['total_surface_water']:.2f} m³")
    print(f"  Max groundwater flow: {final_diag['hydrology']['max_groundwater_flow']:.4f} m/s")
    print()

    # Print summary
    print("=" * 70)
    print("Demo complete!")
    print()
    print("NOTE: This is a stub implementation. Full SE(3)-based hydrological")
    print("dynamics (Darcy flow on manifold, Richards equation, water table")
    print("dynamics) will be implemented in future versions.")
    print("=" * 70)

    return results


if __name__ == "__main__":
    results = run_groundwater_demo()
