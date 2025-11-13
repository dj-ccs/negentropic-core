"""
Physics Solvers Module

This module provides physical simulation solvers for atmospheric, hydrological,
and soil processes, all built on SE(3) Lie group foundations.

Submodules:
    - atmosphere: Atmospheric dynamics and diffusion
    - hydrology: Groundwater flow and surface water dynamics
    - soil: Soil moisture, nutrients, and thermal dynamics
"""

from .atmosphere import AtmosphereSolver
from .hydrology import HydrologySolver
from .soil import SoilSolver

__all__ = ["AtmosphereSolver", "HydrologySolver", "SoilSolver"]
__version__ = "0.1.0-alpha"
