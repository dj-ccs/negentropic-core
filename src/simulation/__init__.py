"""
Simulation Orchestration Module

This module provides the simulation kernel, scheduler, and time integration
infrastructure for coordinating multiple physics solvers.

Components:
    - SimulationKernel: Main orchestrator for multi-physics simulation
    - Scheduler: Time stepping and solver coordination
    - Integrator: SE(3)-aware time integration schemes
"""

from .kernel import SimulationKernel
from .scheduler import Scheduler, SchedulerConfig
from .integrator import SE3Integrator, IntegratorType

__all__ = [
    "SimulationKernel",
    "Scheduler",
    "SchedulerConfig",
    "SE3Integrator",
    "IntegratorType"
]
__version__ = "0.1.0-alpha"
