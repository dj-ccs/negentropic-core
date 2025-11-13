"""
Unit Tests for Regenerative Metrics Service

PROVENANCE:
-----------
Created for: Open Science DLT - Pillar I (Science)
Integration Date: 2025-11-11
Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS

Tests the core functionality of the metrics service API.
"""

import pytest
import numpy as np
import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from metrics_service import (
    compute_regenerative_metrics,
    TrajectoryEncoder,
    RegenerativeMetrics
)

from se3_double_scale import (
    SE3Pose,
    generate_random_trajectory
)


class TestTrajectoryEncoder:
    """Test trajectory encoding from various input formats"""

    def test_encode_from_poses_rotation_vectors(self):
        """Test encoding from rotation vectors"""
        poses = [
            {"rotation": [0.1, 0, 0], "translation": [0.5, 0, 0]},
            {"rotation": [0, 0.1, 0], "translation": [0, 0.5, 0]},
        ]

        trajectory = TrajectoryEncoder.encode_from_poses(poses)

        assert len(trajectory) == 2
        assert trajectory.bounded is True
        assert trajectory.r_max == 1.0

    def test_encode_from_poses_rotation_matrices(self):
        """Test encoding from rotation matrices"""
        poses = [
            {
                "rotation": np.eye(3).tolist(),
                "translation": [0.1, 0.1, 0.1]
            }
        ]

        trajectory = TrajectoryEncoder.encode_from_poses(poses)

        assert len(trajectory) == 1

    def test_encode_from_timeseries(self):
        """Test encoding from time-series data"""
        positions = np.array([
            [0.0, 0.0, 0.0],
            [0.1, 0.1, 0.0],
            [0.2, 0.2, 0.0]
        ])

        orientations = np.array([
            [0.0, 0.0, 0.0],
            [0.1, 0.0, 0.0],
            [0.2, 0.0, 0.0]
        ])

        trajectory = TrajectoryEncoder.encode_from_timeseries(
            positions,
            orientations
        )

        assert len(trajectory) == 3

    def test_encode_from_state_vectors(self):
        """Test encoding from state change vectors"""
        state_vectors = np.random.randn(5, 6) * 0.1

        trajectory = TrajectoryEncoder.encode_from_state_changes(state_vectors)

        assert len(trajectory) == 5


class TestRegenerativeMetrics:
    """Test regenerative metrics computation"""

    def test_compute_metrics_basic(self):
        """Test basic metrics computation"""
        trajectory_data = {
            "poses": [
                {"rotation": [0.1, 0, 0], "translation": [0.1, 0, 0]},
                {"rotation": [0, 0.1, 0], "translation": [0, 0.1, 0]},
                {"rotation": [-0.1, 0, 0], "translation": [-0.1, 0, 0]},
                {"rotation": [0, -0.1, 0], "translation": [0, -0.1, 0]},
            ]
        }

        metrics = compute_regenerative_metrics(trajectory_data)

        # Check required fields
        assert "optimal_lambda" in metrics
        assert "return_error_epsilon" in metrics
        assert "verification_score" in metrics

        # Check types
        assert isinstance(metrics["optimal_lambda"], float)
        assert isinstance(metrics["return_error_epsilon"], float)
        assert isinstance(metrics["verification_score"], float)

        # Check ranges
        assert 0.0 < metrics["optimal_lambda"] < 10.0
        assert metrics["return_error_epsilon"] >= 0.0
        assert 0.0 <= metrics["verification_score"] <= 1.0

    def test_compute_metrics_with_options(self):
        """Test metrics computation with custom options"""
        trajectory_data = {
            "poses": [
                {"rotation": [0.05, 0, 0], "translation": [0.1, 0, 0]},
                {"rotation": [0, 0.05, 0], "translation": [0, 0.1, 0]},
            ]
        }

        metrics = compute_regenerative_metrics(
            trajectory_data,
            enable_resonance_detection=True,
            enable_verification_cascade=True,
            bounded=True,
            r_max=2.0,
            lambda_bounds=(0.3, 1.5)
        )

        assert metrics is not None
        assert "metadata" in metrics
        assert metrics["metadata"]["r_max"] == 2.0

    def test_compute_metrics_resonance_detection(self):
        """Test that resonance detection runs when enabled"""
        trajectory_data = {
            "poses": [
                {"rotation": [0.1, 0, 0], "translation": [0.2, 0, 0]},
                {"rotation": [0, 0.1, 0], "translation": [0, 0.2, 0]},
                {"rotation": [-0.1, 0, 0], "translation": [-0.2, 0, 0]},
            ]
        }

        metrics = compute_regenerative_metrics(
            trajectory_data,
            enable_resonance_detection=True
        )

        # Resonance detection should add resonance_detected field
        assert "resonance_detected" in metrics

    def test_compute_metrics_list_format(self):
        """Test that list input format works"""
        trajectory_data = [
            {"rotation": [0.1, 0, 0], "translation": [0.1, 0, 0]},
            {"rotation": [0, 0.1, 0], "translation": [0, 0.1, 0]},
        ]

        metrics = compute_regenerative_metrics(trajectory_data)

        assert "optimal_lambda" in metrics

    def test_compute_metrics_invalid_input(self):
        """Test error handling for invalid input"""
        with pytest.raises(ValueError):
            compute_regenerative_metrics({})

        with pytest.raises(ValueError):
            compute_regenerative_metrics({"unknown_field": []})


class TestIntegration:
    """Integration tests for complete workflow"""

    def test_random_trajectory_pipeline(self):
        """Test complete pipeline with random trajectory"""
        # Generate random trajectory using core module
        trajectory = generate_random_trajectory(T=10, r_max=1.0)

        # Convert to input format
        poses = []
        for pose in trajectory.poses:
            poses.append({
                "rotation": pose.to_rotation_vector().tolist(),
                "translation": pose.translation.tolist()
            })

        trajectory_data = {"poses": poses}

        # Compute metrics
        metrics = compute_regenerative_metrics(
            trajectory_data,
            enable_resonance_detection=True,
            enable_verification_cascade=True
        )

        # Verify results
        assert metrics["optimal_lambda"] > 0
        assert metrics["verification_score"] >= 0
        assert metrics["confidence"] > 0
        assert metrics["metadata"]["trajectory_length"] == 10

    def test_golden_ratio_preference(self):
        """Test if metrics detect golden ratio preference"""
        # Generate multiple random trajectories
        golden_ratio = (np.sqrt(5) - 1) / 2

        lambdas = []
        for _ in range(5):
            trajectory = generate_random_trajectory(T=8, r_max=1.0)

            poses = [
                {
                    "rotation": pose.to_rotation_vector().tolist(),
                    "translation": pose.translation.tolist()
                }
                for pose in trajectory.poses
            ]

            metrics = compute_regenerative_metrics({"poses": poses})
            lambdas.append(metrics["optimal_lambda"])

        # Check if any λ is close to golden ratio
        near_golden = [
            lam for lam in lambdas
            if abs(lam - golden_ratio) / golden_ratio < 0.3
        ]

        # Document observation (informational test)
        print(f"\nOptimal λ values: {lambdas}")
        print(f"Golden ratio: {golden_ratio:.4f}")
        print(f"Count near golden ratio: {len(near_golden)}/{len(lambdas)}")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
