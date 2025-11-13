#!/usr/bin/env python3
"""
Atmospheric Diffusion Demo

Demonstrates SE(3)-based atmospheric simulation with temperature diffusion
and wind field evolution on a manifold.

Status: Demo using stub implementation (v0.1.0-alpha)
"""

import numpy as np
import sys
from pathlib import Path

# Add src to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from physics import AtmosphereSolver
from simulation import SimulationKernel, SimulationConfig


def create_initial_conditions_hotspot():
    """Create initial conditions with a temperature hotspot."""
    nx, ny, nz = 32, 32, 16

    # Temperature field with Gaussian hotspot
    x = np.linspace(-1, 1, nx)
    y = np.linspace(-1, 1, ny)
    z = np.linspace(0, 1, nz)
    X, Y, Z = np.meshgrid(x, y, z, indexing='ij')

    # Gaussian hotspot centered at (0, 0, 0.5)
    temperature = 288.15 + 20.0 * np.exp(-(X**2 + Y**2 + (Z - 0.5)**2) / 0.1)

    # Uniform pressure
    pressure = np.ones((nx, ny, nz)) * 101325.0

    # No initial wind
    wind_velocity = np.zeros((nx, ny, nz, 3))

    # 50% humidity
    humidity = np.ones((nx, ny, nz)) * 0.5

    return {
        "atmosphere": {
            "temperature": temperature,
            "pressure": pressure,
            "wind_velocity": wind_velocity,
            "humidity": humidity,
            "timestamp": 0.0
        }
    }


def run_atmosphere_demo():
    """Run atmospheric diffusion demonstration."""
    print("=" * 70)
    print("NEGENTROPIC-CORE: Atmospheric Diffusion Demo")
    print("=" * 70)
    print()
    print("This demo simulates temperature diffusion in the atmosphere")
    print("using a stub SE(3)-based solver (to be fully implemented).")
    print()

    # Configure simulation
    config = SimulationConfig(
        grid_size=(32, 32, 16),
        dt=60.0,  # 1 minute time steps
        max_steps=100,
        output_interval=10,
        enable_atmosphere=True,
        enable_hydrology=False,
        enable_soil=False
    )

    # Create kernel
    kernel = SimulationKernel(config)

    # Initialize with hotspot
    initial_conditions = create_initial_conditions_hotspot()
    kernel.initialize(initial_conditions)

    # Get initial diagnostics
    print("Initial conditions:")
    initial_diag = kernel.compute_diagnostics()
    print(f"  Mean temperature: {initial_diag['atmosphere']['mean_temperature']:.2f} K "
          f"({initial_diag['atmosphere']['mean_temperature'] - 273.15:.2f} °C)")
    print(f"  Mean pressure: {initial_diag['atmosphere']['mean_pressure']:.0f} Pa")
    print(f"  Max wind speed: {initial_diag['atmosphere']['max_wind_speed']:.2f} m/s")
    print()

    # Run simulation
    results = kernel.run(save_history=True)

    # Get final diagnostics
    print("\nFinal state:")
    final_diag = results['diagnostics']
    print(f"  Mean temperature: {final_diag['atmosphere']['mean_temperature']:.2f} K "
          f"({final_diag['atmosphere']['mean_temperature'] - 273.15:.2f} °C)")
    print(f"  Mean pressure: {final_diag['atmosphere']['mean_pressure']:.0f} Pa")
    print(f"  Max wind speed: {final_diag['atmosphere']['max_wind_speed']:.2f} m/s")
    print()

    # Print summary
    print("=" * 70)
    print("Demo complete!")
    print()
    print("NOTE: This is a stub implementation. Full SE(3)-based atmospheric")
    print("dynamics (Navier-Stokes on manifold, heat diffusion) will be")
    print("implemented in future versions.")
    print("=" * 70)

    return results


if __name__ == "__main__":
    results = run_atmosphere_demo()
