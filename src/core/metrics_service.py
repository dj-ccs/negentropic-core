"""
Regenerative Metrics Service

PROVENANCE:
-----------
Created for: Open Science DLT - Pillar I (Science)
Integration Date: 2025-11-11
Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS
Purpose: Scientific Application Layer for SE(3) λ-Estimation

This module provides the core service function for computing regenerative metrics
from trajectory data using the SE(3) Double-and-Scale framework from UCF.

MANDATE:
--------
Implement the Scientific Application Layer by exposing core metrics of the
SE(3) Double-and-Scale Regenerative Principle (ADR-0001).

API Contract:
-------------
Input: trajectory_data (dict or list of transformations)
Output: {
    "optimal_lambda": float,           # Optimal scaling factor λ
    "return_error_epsilon": float,     # Return error ε to identity
    "verification_score": float        # Multi-level verification score [0, 1]
}
"""

import numpy as np
from typing import Dict, List, Any, Union, Optional
from dataclasses import dataclass, asdict
import logging

from .se3_double_scale import (
    SE3Pose,
    SE3Trajectory,
    optimize_scaling_factor,
    verify_approximate_return
)

from .resonance_aware import (
    VerificationCascade,
    ResonanceDetector
)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@dataclass
class RegenerativeMetrics:
    """
    Complete regenerative metrics output.

    Attributes:
        optimal_lambda: Optimal scaling factor λ ∈ [0.1, 2.0]
        return_error_epsilon: Frobenius distance to identity after double-and-scale
        verification_score: Multi-level verification score ∈ [0, 1]
        resonance_detected: Name of detected natural resonance (if any)
        confidence: Confidence in metrics (based on optimization convergence)
        metadata: Additional metadata (trajectory length, bounds, etc.)
    """
    optimal_lambda: float
    return_error_epsilon: float
    verification_score: float
    resonance_detected: Optional[str] = None
    confidence: float = 1.0
    metadata: Optional[Dict[str, Any]] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return asdict(self)


class TrajectoryEncoder:
    """
    Encode raw trajectory data into SE(3) trajectory objects.

    Supports multiple input formats:
    1. List of SE(3) poses (rotation + translation)
    2. Time-series sensor data (position + orientation)
    3. Agricultural state data (soil metrics + transformations)
    4. Generic transformation sequences
    """

    @staticmethod
    def encode_from_poses(
        poses: List[Dict[str, Any]],
        bounded: bool = True,
        r_max: float = 1.0
    ) -> SE3Trajectory:
        """
        Encode from explicit pose dictionaries.

        Each pose should contain:
        - 'rotation': 3x3 matrix or 3D rotation vector (axis-angle)
        - 'translation': 3D vector

        Args:
            poses: List of pose dictionaries
            bounded: Whether to enforce translation bounds
            r_max: Maximum translation radius

        Returns:
            SE3Trajectory object
        """
        se3_poses = []

        for pose_data in poses:
            rotation_data = pose_data.get('rotation')
            translation_data = pose_data.get('translation')

            if rotation_data is None or translation_data is None:
                raise ValueError("Each pose must contain 'rotation' and 'translation'")

            # Convert to numpy arrays
            rotation = np.array(rotation_data)
            translation = np.array(translation_data)

            # Handle rotation vector (3D) vs rotation matrix (3x3)
            if rotation.shape == (3,):
                # Rotation vector (axis-angle)
                pose = SE3Pose.from_rotation_vector(rotation, translation)
            elif rotation.shape == (3, 3):
                # Rotation matrix
                pose = SE3Pose(rotation=rotation, translation=translation)
            else:
                raise ValueError(f"Invalid rotation shape: {rotation.shape}. Expected (3,) or (3, 3)")

            se3_poses.append(pose)

        return SE3Trajectory(se3_poses, bounded=bounded, r_max=r_max)

    @staticmethod
    def encode_from_timeseries(
        positions: np.ndarray,
        orientations: np.ndarray,
        bounded: bool = True,
        r_max: float = 1.0
    ) -> SE3Trajectory:
        """
        Encode from time-series sensor data.

        Args:
            positions: (T, 3) array of positions
            orientations: (T, 3) or (T, 4) array of orientations (rotation vectors or quaternions)
            bounded: Whether to enforce translation bounds
            r_max: Maximum translation radius

        Returns:
            SE3Trajectory object
        """
        if len(positions) != len(orientations):
            raise ValueError("Positions and orientations must have same length")

        se3_poses = []

        for i in range(len(positions)):
            pos = positions[i]
            orient = orientations[i]

            # Encode as incremental transformation (relative to previous state)
            if i == 0:
                # First pose: identity with initial position
                pose = SE3Pose.from_rotation_vector(np.zeros(3), pos)
            else:
                # Incremental transformation
                delta_pos = positions[i] - positions[i - 1]
                delta_orient = orientations[i] - orientations[i - 1]

                pose = SE3Pose.from_rotation_vector(delta_orient[:3], delta_pos)

            se3_poses.append(pose)

        return SE3Trajectory(se3_poses, bounded=bounded, r_max=r_max)

    @staticmethod
    def encode_from_state_changes(
        state_vectors: np.ndarray,
        bounded: bool = True,
        r_max: float = 1.0
    ) -> SE3Trajectory:
        """
        Encode from generic state change vectors.

        Maps high-dimensional state changes to SE(3) using dimensionality reduction.
        First 3 dimensions → rotation, next 3 → translation.

        Args:
            state_vectors: (T, D) array of state change vectors (D ≥ 6)
            bounded: Whether to enforce translation bounds
            r_max: Maximum translation radius

        Returns:
            SE3Trajectory object
        """
        if state_vectors.shape[1] < 6:
            raise ValueError("State vectors must have at least 6 dimensions")

        se3_poses = []

        for state in state_vectors:
            rotation_vec = state[:3]
            translation_vec = state[3:6]

            pose = SE3Pose.from_rotation_vector(rotation_vec, translation_vec)
            se3_poses.append(pose)

        return SE3Trajectory(se3_poses, bounded=bounded, r_max=r_max)


def compute_regenerative_metrics(
    trajectory_data: Union[Dict[str, Any], List[Dict[str, Any]]],
    enable_resonance_detection: bool = True,
    enable_verification_cascade: bool = True,
    bounded: bool = True,
    r_max: float = 1.0,
    lambda_bounds: tuple = (0.1, 2.0)
) -> Dict[str, Any]:
    """
    Compute regenerative metrics from trajectory data.

    This is the PRIMARY API FUNCTION specified in the UCF directive.

    Workflow:
    1. Load raw data from input
    2. Encode as SE(3) trajectory
    3. Call optimize_scaling_factor to find optimal λ
    4. Call VerificationCascade to get verification score
    5. Return JSON object: {"optimal_lambda": λ, "return_error_epsilon": ε, "verification_score": Score}

    Args:
        trajectory_data: Input trajectory data (see TrajectoryEncoder for formats)
        enable_resonance_detection: Whether to detect natural resonances (golden ratio, etc.)
        enable_verification_cascade: Whether to run multi-level verification
        bounded: Whether to enforce translation bounds
        r_max: Maximum translation radius
        lambda_bounds: Search bounds for λ optimization

    Returns:
        Dictionary with regenerative metrics:
        {
            "optimal_lambda": float,
            "return_error_epsilon": float,
            "verification_score": float,
            "resonance_detected": str (optional),
            "confidence": float,
            "metadata": dict
        }

    Example:
        >>> trajectory_data = {
        ...     "poses": [
        ...         {"rotation": [0.1, 0, 0], "translation": [0.5, 0, 0]},
        ...         {"rotation": [0, 0.1, 0], "translation": [0, 0.5, 0]},
        ...     ]
        ... }
        >>> metrics = compute_regenerative_metrics(trajectory_data)
        >>> print(f"Optimal λ: {metrics['optimal_lambda']:.4f}")
    """
    try:
        logger.info("Starting regenerative metrics computation")

        # Step 1: Load and encode trajectory
        logger.info("Encoding trajectory data")

        if isinstance(trajectory_data, dict):
            # Dictionary format with explicit structure
            if 'poses' in trajectory_data:
                # Format: {"poses": [...]}
                poses = trajectory_data['poses']
                trajectory = TrajectoryEncoder.encode_from_poses(
                    poses,
                    bounded=bounded,
                    r_max=r_max
                )
            elif 'positions' in trajectory_data and 'orientations' in trajectory_data:
                # Format: {"positions": [...], "orientations": [...]}
                positions = np.array(trajectory_data['positions'])
                orientations = np.array(trajectory_data['orientations'])
                trajectory = TrajectoryEncoder.encode_from_timeseries(
                    positions,
                    orientations,
                    bounded=bounded,
                    r_max=r_max
                )
            elif 'state_vectors' in trajectory_data:
                # Format: {"state_vectors": [...]}
                state_vectors = np.array(trajectory_data['state_vectors'])
                trajectory = TrajectoryEncoder.encode_from_state_changes(
                    state_vectors,
                    bounded=bounded,
                    r_max=r_max
                )
            else:
                raise ValueError("Unsupported trajectory_data format. Expected 'poses', 'positions'+'orientations', or 'state_vectors'")

        elif isinstance(trajectory_data, list):
            # List format: assume list of poses
            trajectory = TrajectoryEncoder.encode_from_poses(
                trajectory_data,
                bounded=bounded,
                r_max=r_max
            )
        else:
            raise ValueError(f"Unsupported trajectory_data type: {type(trajectory_data)}")

        logger.info(f"Encoded trajectory with {len(trajectory)} poses")

        # Step 2: Optimize scaling factor λ
        logger.info("Optimizing scaling factor λ")

        result = optimize_scaling_factor(
            trajectory,
            lambda_bounds=lambda_bounds,
            double=True,
            method='bounded'
        )

        optimal_lambda = float(result.x)
        return_error_epsilon = float(result.fun)

        logger.info(f"Optimal λ: {optimal_lambda:.4f}, Return error ε: {return_error_epsilon:.6f}")

        # Step 3: Resonance detection (optional)
        resonance_detected = None
        if enable_resonance_detection:
            logger.info("Detecting natural resonances")
            detector = ResonanceDetector()
            resonance_result = detector.detect_natural_scaling(trajectory)

            if resonance_result.is_natural:
                resonance_detected = resonance_result.best_resonance
                logger.info(f"Resonance detected: {resonance_detected}")

        # Step 4: Verification cascade (compute verification score)
        verification_score = 0.0

        if enable_verification_cascade:
            logger.info("Running verification cascade")
            cascade = VerificationCascade()
            verification_result = cascade.verify_regeneration(
                trajectory,
                optimal_lambda,
                base_token_amount=100.0
            )

            verification_score = float(verification_result.overall_score)
            logger.info(f"Verification score: {verification_score:.4f}")

        # Step 5: Compute confidence based on optimization convergence
        confidence = 1.0
        if result.success:
            # High confidence if optimization converged
            confidence = min(1.0, 1.0 / (1.0 + return_error_epsilon))
        else:
            # Lower confidence if optimization did not converge
            confidence = 0.5
            logger.warning("Optimization did not fully converge")

        # Step 6: Assemble metadata
        metadata = {
            "trajectory_length": len(trajectory),
            "bounded": bounded,
            "r_max": r_max,
            "lambda_bounds": lambda_bounds,
            "optimization_success": result.success,
            "optimization_iterations": getattr(result, 'nfev', None),
            "timestamp": np.datetime64('now').astype(str)
        }

        # Step 7: Create result object
        metrics = RegenerativeMetrics(
            optimal_lambda=optimal_lambda,
            return_error_epsilon=return_error_epsilon,
            verification_score=verification_score,
            resonance_detected=resonance_detected,
            confidence=confidence,
            metadata=metadata
        )

        logger.info("Regenerative metrics computation complete")

        return metrics.to_dict()

    except Exception as e:
        logger.error(f"Error computing regenerative metrics: {str(e)}", exc_info=True)
        raise


def compute_batch_metrics(
    trajectories: List[Dict[str, Any]],
    **kwargs
) -> List[Dict[str, Any]]:
    """
    Compute regenerative metrics for multiple trajectories in batch.

    Args:
        trajectories: List of trajectory data dictionaries
        **kwargs: Additional arguments passed to compute_regenerative_metrics

    Returns:
        List of metrics dictionaries
    """
    results = []

    for i, trajectory_data in enumerate(trajectories):
        logger.info(f"Processing trajectory {i + 1}/{len(trajectories)}")
        try:
            metrics = compute_regenerative_metrics(trajectory_data, **kwargs)
            results.append(metrics)
        except Exception as e:
            logger.error(f"Error processing trajectory {i + 1}: {str(e)}")
            results.append({
                "error": str(e),
                "trajectory_index": i
            })

    return results
