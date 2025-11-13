#!/usr/bin/env python3
"""
Soil Moisture and Carbon Cycling Demo

Demonstrates SE(3)-based soil simulation with moisture transport,
thermal dynamics, nutrient diffusion, and carbon cycling.

Status: Demo using stub implementation (v0.1.0-alpha)
"""

import numpy as np
import sys
from pathlib import Path

# Add src to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from physics import SoilSolver
from simulation import SimulationKernel, SimulationConfig


def create_initial_conditions_irrigation():
    """Create initial conditions with irrigation event."""
    nx, ny, nz = 32, 32, 8

    # Soil moisture with dry surface
    soil_moisture = np.ones((nx, ny, nz)) * 0.15  # 15% (dry)

    # Add irrigation patch (wetted area)
    x = np.linspace(-1, 1, nx)
    y = np.linspace(-1, 1, ny)
    X, Y = np.meshgrid(x, y, indexing='ij')
    irrigation_mask = (X**2 + Y**2) < 0.5

    # Increased moisture in top layers from irrigation
    for iz in range(3):  # Top 3 layers
        soil_moisture[:, :, iz][irrigation_mask] = 0.40  # 40% (irrigated)

    # Temperature (warm surface, cooler at depth)
    temperature = np.ones((nx, ny, nz)) * 288.15  # 15°C baseline
    temperature[:, :, 0] += 5.0  # Surface warmer

    # Nutrients (uniform baseline)
    nutrients = {
        "nitrogen": np.ones((nx, ny, nz)) * 0.01,      # 0.01 kg/m³
        "phosphorus": np.ones((nx, ny, nz)) * 0.005,   # 0.005 kg/m³
        "potassium": np.ones((nx, ny, nz)) * 0.02,     # 0.02 kg/m³
    }

    # Soil organic carbon (higher near surface)
    carbon = np.ones((nx, ny, nz)) * 0.02  # 0.02 kg/m³ baseline
    for iz in range(3):
        carbon[:, :, iz] *= (1.5 + iz * 0.3)  # More carbon near surface

    return {
        "soil": {
            "moisture": soil_moisture,
            "temperature": temperature,
            "nutrients": nutrients,
            "carbon": carbon,
            "timestamp": 0.0
        }
    }


def run_soil_moisture_demo():
    """Run soil moisture and carbon cycling demonstration."""
    print("=" * 70)
    print("NEGENTROPIC-CORE: Soil Moisture & Carbon Cycling Demo")
    print("=" * 70)
    print()
    print("This demo simulates soil moisture infiltration and carbon dynamics")
    print("following an irrigation event, using a stub SE(3)-based solver")
    print("(to be fully implemented).")
    print()

    # Configure simulation
    config = SimulationConfig(
        grid_size=(32, 32, 8),
        dt=3600.0,  # 1 hour time steps
        max_steps=72,  # 72 hours (3 days)
        output_interval=12,  # Output every 12 hours
        enable_atmosphere=False,
        enable_hydrology=False,
        enable_soil=True
    )

    # Create kernel
    kernel = SimulationKernel(config)

    # Initialize with irrigation event
    initial_conditions = create_initial_conditions_irrigation()
    kernel.initialize(initial_conditions)

    # Get initial diagnostics
    print("Initial conditions (post-irrigation):")
    initial_diag = kernel.compute_diagnostics()
    print(f"  Mean soil moisture: {initial_diag['soil']['mean_moisture']:.3f}")
    print(f"  Mean temperature: {initial_diag['soil']['mean_temperature']:.2f} K "
          f"({initial_diag['soil']['mean_temperature'] - 273.15:.2f} °C)")
    print(f"  Total soil carbon: {initial_diag['soil']['total_carbon']:.2f} kg")
    print(f"  Total nitrogen: {initial_diag['soil']['total_nitrogen']:.3f} kg")
    print(f"  Total phosphorus: {initial_diag['soil']['total_phosphorus']:.3f} kg")
    print(f"  Total potassium: {initial_diag['soil']['total_potassium']:.3f} kg")
    print()

    # Run simulation
    results = kernel.run(save_history=True)

    # Get final diagnostics
    print("\nFinal state (after 3 days):")
    final_diag = results['diagnostics']
    print(f"  Mean soil moisture: {final_diag['soil']['mean_moisture']:.3f}")
    print(f"  Mean temperature: {final_diag['soil']['mean_temperature']:.2f} K "
          f"({final_diag['soil']['mean_temperature'] - 273.15:.2f} °C)")
    print(f"  Total soil carbon: {final_diag['soil']['total_carbon']:.2f} kg")
    print(f"  Total nitrogen: {final_diag['soil']['total_nitrogen']:.3f} kg")
    print(f"  Total phosphorus: {final_diag['soil']['total_phosphorus']:.3f} kg")
    print(f"  Total potassium: {final_diag['soil']['total_potassium']:.3f} kg")
    print()

    # Print history if available
    if results['history']:
        print("\nMoisture evolution:")
        for record in results['history']:
            moisture = record['diagnostics']['soil']['mean_moisture']
            hours = record['time'] / 3600.0
            print(f"  t = {hours:5.1f}h: moisture = {moisture:.3f}")

    # Print summary
    print()
    print("=" * 70)
    print("Demo complete!")
    print()
    print("NOTE: This is a stub implementation. Full SE(3)-based soil dynamics")
    print("(Richards equation, heat transport with phase change, nutrient")
    print("diffusion, carbon decomposition) will be implemented in future")
    print("versions.")
    print("=" * 70)

    return results


if __name__ == "__main__":
    results = run_soil_moisture_demo()
