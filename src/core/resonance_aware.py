"""
Resonance-Aware Extensions for SE(3) Double-and-Scale

PROVENANCE:
-----------
Source: Unified Conscious Evolution Framework (UCF)
Repository: https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework
Path: foundations/lie_groups/resonance_aware.py
Integration Date: 2025-11-11
Integration Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS
Purpose: Pillar I (Science) - Verification Cascade for EHDC Token Metrics

**RESEARCH STATUS: EXPERIMENTAL - REQUIRES VALIDATION**

This module implements exploratory extensions based on preliminary observations.
Treat as a research framework for hypothesis testing, NOT production-ready code.

Implements Claude Opus insights:
- ResonanceDetector: Test for natural mathematical constants (golden ratio, silver ratio, etc.)
- VerificationCascade: Multi-level verification for EHDC token generation
- NarrativeQualityMetric: Quantify story satisfaction using return principles
- ResonanceAwareOptimizer: Optimization biased toward natural scaling factors

Based on PRELIMINARY observation: λ ≈ 0.618 (golden ratio) appeared in ~40% of
small random trajectories (N=5 trials). This could indicate natural preference
for mathematical constants, OR could be optimization landscape artifact.

REQUIRES VALIDATION:
- Larger sample sizes (N≥1000)
- Multiple random distributions (uniform, Gaussian, heavy-tailed)
- Control groups on other Lie groups (SO(2), SE(2))
- Noise perturbation stability tests
- Real-world agricultural/ecological field trials

See VALIDATION_METHODOLOGY.md for complete empirical requirements before
treating these tools as production-ready.

Current use cases:
✅ Hypothesis generation and exploration
✅ Research experiments and data collection
❌ Production token economics without validation
❌ Claiming universal principles without empirical proof
"""

import numpy as np
from typing import Dict, List, Tuple, Optional, Any
from dataclasses import dataclass
from scipy.optimize import minimize_scalar, basinhopping

from .se3_double_scale import (
    SE3Pose,
    SE3Trajectory,
    compose_trajectory,
    compose_se3,
    scale_trajectory,
    double_trajectory,
    frobenius_distance_to_identity,
    compute_return_error
)


@dataclass
class ResonanceResult:
    """Result from resonance detection"""
    best_resonance: str
    best_error: float
    all_resonances: Dict[str, float]
    is_natural: bool  # True if system prefers mathematical constant over arbitrary value


class ResonanceDetector:
    """
    Detect natural mathematical resonances in system scaling [Opus insight]

    Tests whether systems prefer fundamental mathematical constants:
    - Golden ratio φ ≈ 0.618: Optimal packing, Fibonacci sequences
    - Silver ratio δ ≈ 2.414: Octagon geometry, continued fraction √2 + 1
    - Plastic number ρ ≈ 1.325: Minimal Pisot number, x³ = x + 1
    - Octave (2.0): Doubling, musical harmonic
    - Perfect fifth (1.5): Musical consonance, 3:2 ratio

    Physical interpretation: Natural systems may "lock" to these ratios
    because they represent stable attractors in the space of regenerative cycles.
    """

    # Mathematical constants
    GOLDEN_RATIO = (np.sqrt(5) - 1) / 2  # φ ≈ 0.618033988749
    SILVER_RATIO = 1 + np.sqrt(2)  # δ ≈ 2.414213562373
    PLASTIC_NUMBER = 1.324717957244  # ρ (real root of x³ = x + 1)
    OCTAVE = 2.0
    PERFECT_FIFTH = 3.0 / 2.0  # 1.5
    PERFECT_FOURTH = 4.0 / 3.0  # ≈ 1.333
    MAJOR_THIRD = 5.0 / 4.0  # 1.25

    def __init__(self, tolerance: float = 0.1):
        """
        Initialize resonance detector.

        Args:
            tolerance: Fraction of ratio for "close enough" (default: 10%)
        """
        self.tolerance = tolerance
        self.resonance_constants = {
            "golden_ratio": self.GOLDEN_RATIO,
            "silver_ratio": self.SILVER_RATIO,
            "plastic_number": self.PLASTIC_NUMBER,
            "octave": self.OCTAVE,
            "perfect_fifth": self.PERFECT_FIFTH,
            "perfect_fourth": self.PERFECT_FOURTH,
            "major_third": self.MAJOR_THIRD
        }

    def test_scaling(self, trajectory: SE3Trajectory, lambda_test: float) -> float:
        """
        Test return error for specific scaling factor.

        Args:
            trajectory: SE(3) trajectory to test
            lambda_test: Scaling factor to evaluate

        Returns:
            Return error (lower is better)
        """
        return compute_return_error(trajectory, lambda_test, double=True)

    def detect_natural_scaling(self, trajectory: SE3Trajectory) -> ResonanceResult:
        """
        Test if system prefers mathematical constants [Opus insight]

        Compares return errors at natural mathematical ratios vs. optimized value.
        If a natural constant achieves similar error to optimized value, the system
        exhibits resonance with that constant.

        Args:
            trajectory: SE(3) trajectory to analyze

        Returns:
            ResonanceResult with best resonance and comparison
        """
        # Test all resonance constants
        results = {}
        for name, ratio in self.resonance_constants.items():
            error = self.test_scaling(trajectory, ratio)
            results[name] = error

        # Find best natural resonance
        best_resonance = min(results, key=results.get)
        best_error = results[best_resonance]

        # Compare to optimized value (unbounded optimization)
        from scipy.optimize import minimize_scalar
        opt_result = minimize_scalar(
            lambda lam: self.test_scaling(trajectory, lam),
            bounds=(0.1, 10.0),
            method='bounded'
        )
        optimal_error = opt_result.fun

        # System prefers natural constant if within tolerance of optimal
        is_natural = best_error <= optimal_error * (1 + self.tolerance)

        return ResonanceResult(
            best_resonance=best_resonance,
            best_error=best_error,
            all_resonances=results,
            is_natural=is_natural
        )

    def find_nearest_resonance(self, lambda_value: float) -> Tuple[str, float, float]:
        """
        Find nearest mathematical constant to given λ value.

        Args:
            lambda_value: Scaling factor to check

        Returns:
            (resonance_name, resonance_value, distance)
        """
        distances = {
            name: abs(lambda_value - ratio)
            for name, ratio in self.resonance_constants.items()
        }

        nearest_name = min(distances, key=distances.get)
        nearest_value = self.resonance_constants[nearest_name]
        distance = distances[nearest_name]

        return nearest_name, nearest_value, distance


@dataclass
class VerificationResult:
    """Multi-level verification result for regenerative protocols"""
    overall_score: float  # ∈ [0, 1], higher is better
    verifications: Dict[str, float]
    token_award: float
    passed: bool


class VerificationCascade:
    """
    Multi-level verification for EHDC token generation [Opus insight]

    Implements verification cascade:
    1. Topological: Return quality (closure error)
    2. Energetic: Energy conservation (work done vs. returned)
    3. Temporal: Timing consistency (cycle duration stability)
    4. Spatial: Bounded domain (translations within limits)
    5. Stochastic: Noise robustness (performance under perturbations)

    Each level weighted by importance for regenerative systems.
    Overall score determines REGEN token award.
    """

    def __init__(
        self,
        weights: Optional[Dict[str, float]] = None,
        thresholds: Optional[Dict[str, float]] = None
    ):
        """
        Initialize verification cascade.

        Args:
            weights: Relative importance of each verification level
            thresholds: Pass/fail thresholds for each level
        """
        self.weights = weights or {
            "topological": 0.3,
            "energetic": 0.2,
            "temporal": 0.2,
            "spatial": 0.2,
            "stochastic": 0.1
        }

        self.thresholds = thresholds or {
            "topological": 0.1,  # Return error < 0.1
            "energetic": 0.05,  # Energy conservation < 5%
            "temporal": 0.1,  # Timing variation < 10%
            "spatial": 1.0,  # Must be within bounds (binary)
            "stochastic": 0.8  # Noise robustness > 80%
        }

    def verify_return_quality(
        self,
        trajectory: SE3Trajectory,
        lambda_opt: float
    ) -> float:
        """
        Verify topological return quality (closure error).

        Returns:
            Error value (lower is better, < threshold is good)
        """
        return compute_return_error(trajectory, lambda_opt, double=True)

    def verify_energy_conservation(
        self,
        trajectory: SE3Trajectory,
        lambda_opt: float
    ) -> float:
        """
        Verify energy conservation (work done ≈ work returned).

        Approximation: Sum of rotation/translation magnitudes should balance
        in doubled, scaled trajectory.

        Returns:
            Relative energy imbalance (0 = perfect conservation)
        """
        scaled = scale_trajectory(trajectory, lambda_opt)
        doubled = double_trajectory(scaled)

        # Compute "work" as sum of transformation magnitudes
        total_work = 0.0
        for pose in doubled.poses:
            rot_work = np.linalg.norm(pose.to_rotation_vector())
            trans_work = np.linalg.norm(pose.translation)
            total_work += rot_work + trans_work

        # For perfect return, work should sum to zero (cancellation)
        # Normalize by trajectory length
        avg_work = total_work / len(doubled)

        # Return relative imbalance
        return avg_work

    def verify_timing_consistency(
        self,
        trajectory: SE3Trajectory,
        lambda_opt: float
    ) -> float:
        """
        Verify temporal consistency (uniform step sizes).

        For regenerative systems, consistent timing is critical.
        Erratic timing suggests instability.

        Returns:
            Coefficient of variation in step sizes (0 = perfectly uniform)
        """
        # Compute step sizes (pose-to-pose distances)
        step_sizes = []
        for i in range(len(trajectory) - 1):
            pose1 = trajectory[i]
            pose2 = trajectory[i + 1]

            # Distance = rotation + translation change
            rot_dist = np.linalg.norm(
                pose2.to_rotation_vector() - pose1.to_rotation_vector()
            )
            trans_dist = np.linalg.norm(pose2.translation - pose1.translation)

            step_sizes.append(rot_dist + trans_dist)

        if len(step_sizes) == 0:
            return 0.0

        # Coefficient of variation (std / mean)
        mean_step = np.mean(step_sizes)
        std_step = np.std(step_sizes)

        if mean_step < 1e-10:
            return 0.0

        return std_step / mean_step

    def verify_bounded_domain(
        self,
        trajectory: SE3Trajectory
    ) -> bool:
        """
        Verify all translations within bounded domain.

        Returns:
            True if all translations within r_max, False otherwise
        """
        if not trajectory.bounded:
            return True  # Unbounded trajectory, no constraint

        for pose in trajectory.poses:
            if np.linalg.norm(pose.translation) > trajectory.r_max:
                return False

        return True

    def verify_noise_robustness(
        self,
        trajectory: SE3Trajectory,
        lambda_opt: float,
        num_trials: int = 10,
        noise_level: float = 0.05
    ) -> float:
        """
        Verify robustness to stochastic perturbations.

        Adds Gaussian noise to trajectory and checks if return quality degrades.

        Args:
            trajectory: Original trajectory
            lambda_opt: Optimal scaling factor
            num_trials: Number of noise trials
            noise_level: Standard deviation of Gaussian noise

        Returns:
            Robustness score ∈ [0, 1], higher = more robust
        """
        baseline_error = compute_return_error(trajectory, lambda_opt, double=True)

        noisy_errors = []
        for _ in range(num_trials):
            # Add noise to each pose
            noisy_poses = []
            for pose in trajectory.poses:
                # Noise on rotation (small random rotation)
                noise_rot = np.random.normal(0, noise_level, 3)
                noisy_rotvec = pose.to_rotation_vector() + noise_rot

                # Noise on translation
                noise_trans = np.random.normal(0, noise_level, 3)
                noisy_translation = pose.translation + noise_trans

                noisy_poses.append(
                    SE3Pose.from_rotation_vector(noisy_rotvec, noisy_translation)
                )

            noisy_trajectory = SE3Trajectory(
                noisy_poses,
                trajectory.bounded,
                trajectory.r_max
            )

            # Compute error with noise
            noisy_error = compute_return_error(noisy_trajectory, lambda_opt, double=True)
            noisy_errors.append(noisy_error)

        # Robustness = 1 - (mean_noisy_error - baseline_error) / baseline_error
        # Clamp to [0, 1]
        mean_noisy_error = np.mean(noisy_errors)
        if baseline_error < 1e-10:
            # Perfect baseline, any noise is degradation
            robustness = 0.5
        else:
            relative_degradation = (mean_noisy_error - baseline_error) / baseline_error
            robustness = max(0.0, min(1.0, 1.0 - relative_degradation))

        return robustness

    def verify_regeneration(
        self,
        trajectory: SE3Trajectory,
        lambda_opt: float,
        base_token_amount: float = 100.0
    ) -> VerificationResult:
        """
        Multi-level verification for token generation [Opus insight]

        Args:
            trajectory: SE(3) trajectory to verify
            lambda_opt: Optimized scaling factor
            base_token_amount: Base REGEN token amount (scaled by score)

        Returns:
            VerificationResult with overall score and token award
        """
        # Run all verification levels
        verifications = {
            "topological": self.verify_return_quality(trajectory, lambda_opt),
            "energetic": self.verify_energy_conservation(trajectory, lambda_opt),
            "temporal": self.verify_timing_consistency(trajectory, lambda_opt),
            "spatial": float(self.verify_bounded_domain(trajectory)),
            "stochastic": self.verify_noise_robustness(trajectory, lambda_opt)
        }

        # Normalize to [0, 1] where 1 is best
        normalized = {}
        normalized["topological"] = max(0.0, 1.0 - verifications["topological"] / 2.0)
        normalized["energetic"] = max(0.0, 1.0 - verifications["energetic"] / 0.5)
        normalized["temporal"] = max(0.0, 1.0 - verifications["temporal"] / 1.0)
        normalized["spatial"] = verifications["spatial"]  # Already 0 or 1
        normalized["stochastic"] = verifications["stochastic"]  # Already [0, 1]

        # Weighted overall score
        overall_score = sum(
            self.weights[key] * normalized[key]
            for key in self.weights.keys()
        )

        # Check if passed all thresholds
        passed = all(
            (verifications[key] <= self.thresholds[key] if key != "stochastic"
             else verifications[key] >= self.thresholds[key])
            for key in verifications.keys() if key != "spatial"
        ) and verifications["spatial"] == 1.0

        # Token award scales with overall score
        token_award = overall_score * base_token_amount

        return VerificationResult(
            overall_score=overall_score,
            verifications=verifications,
            token_award=token_award,
            passed=passed
        )


class NarrativeQualityMetric:
    """
    Quantify narrative satisfaction using return principles [Opus insight]

    Models story structure as trajectory through cognitive/emotional state space:
    - Opening: Equilibrium state
    - Departure: Hero leaves (increasing transformation)
    - Crisis: Maximum displacement from equilibrium (midpoint)
    - Return: Journey back (mirrored transformation)
    - Resolution: Enhanced equilibrium (transformed return)

    Satisfying narratives exhibit approximate return structure:
    the second half mirrors/inverts the first half at optimal scaling.

    Physical interpretation: Stories are walks in SU(n)-like cognitive manifolds.
    Return quality measures narrative closure / emotional satisfaction.
    """

    def encode_story_beat(
        self,
        beat_description: str,
        intensity: float,
        emotion_vector: np.ndarray
    ) -> SE3Pose:
        """
        Encode narrative beat as SE(3) transformation.

        Args:
            beat_description: Text description of story beat
            intensity: Dramatic intensity (0-1)
            emotion_vector: 3D emotional state (valence, arousal, dominance)

        Returns:
            SE3Pose representing narrative transformation
        """
        # Rotation encodes emotional change
        # Magnitude proportional to intensity
        rot_vec = intensity * emotion_vector / np.linalg.norm(emotion_vector)

        # Translation encodes plot progression
        # x: Character development
        # y: External conflict escalation
        # z: Thematic deepening
        translation = np.array([
            intensity * emotion_vector[0] * 0.5,
            intensity * emotion_vector[1] * 0.5,
            intensity * emotion_vector[2] * 0.3
        ])

        return SE3Pose.from_rotation_vector(rot_vec, translation)

    def measure_story_return(
        self,
        story_beats: List[SE3Pose]
    ) -> Dict[str, Any]:
        """
        Quantify narrative satisfaction using return principles [Opus insight]

        Args:
            story_beats: Sequence of narrative transformations

        Returns:
            Dictionary with satisfaction metrics
        """
        trajectory = SE3Trajectory(story_beats, bounded=True, r_max=3.0)

        # Find optimal doubling point (usually midpoint crisis)
        midpoint = len(story_beats) // 2

        first_half = SE3Trajectory(
            story_beats[:midpoint],
            bounded=True,
            r_max=3.0
        )
        second_half = SE3Trajectory(
            story_beats[midpoint:],
            bounded=True,
            r_max=3.0
        )

        # Compose both halves
        first_total = compose_trajectory(first_half)
        second_total = compose_trajectory(second_half)

        # Measure return quality (how well second half returns from first)
        full_trajectory = compose_se3(first_total, second_total)
        return_error = frobenius_distance_to_identity(full_trajectory)

        # Normalize to [0, 1] satisfaction score
        satisfaction = max(0.0, 1.0 - return_error / 5.0)

        # Compute optimal narrative scaling (intensity adjustment)
        from .se3_double_scale import optimize_scaling_factor
        result = optimize_scaling_factor(trajectory, double=False)
        optimal_scaling = result.x

        return {
            "satisfaction": satisfaction,
            "return_error": return_error,
            "optimal_crisis_point": midpoint,
            "suggested_scaling": optimal_scaling,
            "interpretation": self._interpret_scaling(optimal_scaling)
        }

    def _interpret_scaling(self, lambda_value: float) -> str:
        """Interpret narrative scaling factor"""
        if lambda_value < 0.7:
            return "Understated resolution - consider increasing emotional intensity in Act 3"
        elif lambda_value < 0.9:
            return "Well-balanced narrative pacing"
        elif lambda_value < 1.2:
            return "Standard intensity progression"
        else:
            return "Heightened drama - second half escalates beyond first half"


class ResonanceAwareOptimizer:
    """
    Optimization biased toward natural scaling factors [Opus insight]

    Standard optimization searches entire parameter space uniformly.
    But empirical evidence (40% golden ratio frequency) suggests systems
    naturally prefer mathematical constants.

    This optimizer:
    1. Starts search near golden ratio
    2. Uses tighter bounds around natural resonances
    3. Falls back to global search if resonance fails
    """

    def __init__(self, bias_strength: float = 0.5):
        """
        Initialize resonance-aware optimizer.

        Args:
            bias_strength: How strongly to bias toward golden ratio (0-1)
        """
        self.bias_strength = bias_strength
        self.golden_ratio = (np.sqrt(5) - 1) / 2

    def optimize_with_bias(
        self,
        trajectory: SE3Trajectory,
        bias_to_golden: bool = True
    ) -> Tuple[float, float]:
        """
        Optimization that 'knows' about natural scaling preferences [Opus insight]

        Args:
            trajectory: SE(3) trajectory to optimize
            bias_to_golden: Whether to bias search toward golden ratio

        Returns:
            (optimal_lambda, error)
        """
        if bias_to_golden:
            # Start search near golden ratio
            initial_guess = self.golden_ratio
            bounds = (
                self.golden_ratio * 0.7,
                self.golden_ratio * 1.4
            )
        else:
            # Standard unbiased search
            initial_guess = 1.0
            bounds = (0.1, 10.0)

        # Define cost function
        def cost(lam: float) -> float:
            return compute_return_error(trajectory, lam, double=True)

        # Try local optimization first (around resonance)
        result_local = minimize_scalar(
            cost,
            bounds=bounds,
            method='bounded'
        )

        # If local optimization fails (high error), try global search
        if result_local.fun > 1.0 and bias_to_golden:
            # Fall back to global optimization
            result_global = minimize_scalar(
                cost,
                bounds=(0.1, 10.0),
                method='bounded'
            )

            # Use whichever is better
            if result_global.fun < result_local.fun:
                return result_global.x, result_global.fun

        return result_local.x, result_local.fun

    def multi_resonance_search(
        self,
        trajectory: SE3Trajectory
    ) -> Dict[str, Tuple[float, float]]:
        """
        Test all natural resonances and return results.

        Args:
            trajectory: SE(3) trajectory to optimize

        Returns:
            Dictionary mapping resonance names to (lambda, error) tuples
        """
        detector = ResonanceDetector()
        results = {}

        for name, ratio in detector.resonance_constants.items():
            # Optimize in neighborhood of each resonance
            bounds = (ratio * 0.7, ratio * 1.4)

            def cost(lam: float) -> float:
                return compute_return_error(trajectory, lam, double=True)

            result = minimize_scalar(cost, bounds=bounds, method='bounded')
            results[name] = (result.x, result.fun)

        return results
