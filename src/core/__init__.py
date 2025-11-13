"""
Lie Dynamics Module for Open Science DLT

PROVENANCE:
-----------
Integrated from: Unified Conscious Evolution Framework (UCF)
Repository: https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework
Integration Date: 2025-11-11
Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS

This module provides the SE(3) double-and-scale regenerative dynamics
framework for scientific applications in the open-science-dlt platform.

Modules:
--------
- se3_double_scale: Core SE(3) operations and λ optimization
- resonance_aware: Experimental verification cascade and resonance detection
- metrics_service: REST API service for regenerative metrics computation
"""

from .se3_double_scale import (
    SE3Pose,
    SE3Trajectory,
    compose_se3,
    compose_trajectory,
    scale_se3_pose,
    scale_trajectory,
    double_trajectory,
    frobenius_distance_to_identity,
    compute_return_error,
    optimize_scaling_factor,
    generate_random_trajectory,
    verify_approximate_return,
    TetheredSE3Walker,
    predict_intervention_interference,
    IntegratorType
)

from .resonance_aware import (
    ResonanceDetector,
    ResonanceResult,
    VerificationCascade,
    VerificationResult,
    NarrativeQualityMetric,
    ResonanceAwareOptimizer
)

__version__ = "1.0.0"
__author__ = "UCF Core Team + Open Science DLT"
__license__ = "MIT"

__all__ = [
    # Core SE(3) types
    "SE3Pose",
    "SE3Trajectory",
    "IntegratorType",

    # Core SE(3) operations
    "compose_se3",
    "compose_trajectory",
    "scale_se3_pose",
    "scale_trajectory",
    "double_trajectory",

    # Metrics and optimization
    "frobenius_distance_to_identity",
    "compute_return_error",
    "optimize_scaling_factor",
    "verify_approximate_return",

    # Utilities
    "generate_random_trajectory",
    "TetheredSE3Walker",
    "predict_intervention_interference",

    # Resonance detection (EXPERIMENTAL)
    "ResonanceDetector",
    "ResonanceResult",
    "VerificationCascade",
    "VerificationResult",
    "NarrativeQualityMetric",
    "ResonanceAwareOptimizer",
]
