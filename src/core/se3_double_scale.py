"""
SE(3) Double-and-Scale Approximate Return Module

PROVENANCE:
-----------
Source: Unified Conscious Evolution Framework (UCF)
Repository: https://github.com/dj-ccs/Unified-Conscious-Evolution-Framework
Path: foundations/lie_groups/se3_double_scale.py
Integration Date: 2025-11-11
Integration Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS
Purpose: Pillar I (Science) - λ-Estimation Service for Regenerative Metrics

This module implements the mathematical principle that regenerative systems achieve
stability not through reversal but through transformed return. Based on the discovery
that walks in rotation spaces return home when doubled and scaled.

Theoretical Foundation:
----------------------
While single traversal of a rotation sequence almost never returns to identity
(zero-dimensional point with codimension 3), doubling the sequence while scaling
creates a universal return mechanism. The set of double-identity roots forms a
2D manifold with codimension 1, dramatically increasing return probability.

The Haar measure on SO(3) transforms from biased P(ω) = (1 - cos ω)/π to
uniform P²(ω) = 1/(2π), enabling the "geometric magic" of the reset mechanism.

SE(3) Extension:
---------------
SE(3) = SO(3) ⋉ ℝ³ is non-compact due to translation component, requiring:
- Bounded domains (r_max) to constrain translation
- Separate scaling for rotation (via Lie algebra) and translation (linear)
- Structure-preserving Lie group integrators for accurate trajectory integration

References:
----------
[1.1] Chandrasekaran et al. (2025). Unified Framework for Consensus and
      Synchronization on Lie Groups. IEEE Trans. Automatic Control, 70(11), 7718-7724.
[2.1] Sarlette (2007). Towards coordination algorithms on compact lie groups.
[2.2] Sarlette (2007). Coordination on compact homogeneous manifolds, SO(n), Grassmannians.
[2.3] Sarlette (2007). Discrete approximations, quaternion caveats, SO(3) trajectories.
[3.1] Guivarc'h & Raja (2012). Recurrence and ergodicity of random walks on linear
      groups. Ergodic Theory & Dyn. Sys., 32(4), 1313-1349.
[4.1] Diaconis (1988). Random Walks on Groups. ArXiv, 17-68.
[5.1] Barrau & Bonnabel (2018). Stochastic observers on Lie groups: a tutorial.
      IEEE CDC, 1264-1269.

Applications:
------------
- Agricultural rotation cycles (hemp-wheat-hemp-wheat with scaled interventions)
- Carbon sequestration protocols (paired, scaled applications)
- Digital twin verification (double-measurement with time scaling)
- Multi-agent coordination (synchronized returns in state space)
- Regenerative narrative structures (story arcs with transformed resolution)
"""

import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.optimize import minimize_scalar, OptimizeResult
from typing import List, Tuple, Optional, Dict, Callable
from dataclasses import dataclass
from enum import Enum


class IntegratorType(Enum):
    """Lie group integrator types for structure-preserving integration [2.3]"""
    EXPONENTIAL = "exponential"  # Direct exponential map (simple, fast)
    CROUCH_GROSSMAN = "crouch_grossman"  # Structure-preserving RK method
    MUNTHE_KAAS = "munthe_kaas"  # Lie algebra-based RK method


@dataclass
class SE3Pose:
    """
    SE(3) rigid body pose: rotation + translation [1.1]

    Represents g = [R p; 0 1] where R ∈ SO(3), p ∈ ℝ³

    Attributes:
        rotation: 3x3 orthogonal matrix with det(R) = 1
        translation: 3D position vector
    """
    rotation: np.ndarray  # 3x3 matrix
    translation: np.ndarray  # 3D vector

    def __post_init__(self):
        """Validate SE(3) constraints"""
        assert self.rotation.shape == (3, 3), "Rotation must be 3x3 matrix"
        assert self.translation.shape == (3,), "Translation must be 3D vector"
        assert np.allclose(np.linalg.det(self.rotation), 1.0, atol=1e-6), \
            "Rotation determinant must be 1"
        assert np.allclose(self.rotation @ self.rotation.T, np.eye(3), atol=1e-6), \
            "Rotation must be orthogonal"

    @staticmethod
    def identity() -> 'SE3Pose':
        """Return identity element of SE(3)"""
        return SE3Pose(rotation=np.eye(3), translation=np.zeros(3))

    @staticmethod
    def from_rotation_vector(rot_vec: np.ndarray, translation: np.ndarray) -> 'SE3Pose':
        """Create SE(3) pose from rotation vector (axis-angle) [2.3]"""
        rotation = R.from_rotvec(rot_vec).as_matrix()
        return SE3Pose(rotation=rotation, translation=translation)

    def to_rotation_vector(self) -> np.ndarray:
        """Extract rotation as axis-angle vector [2.3]"""
        return R.from_matrix(self.rotation).as_rotvec()

    def to_quaternion(self) -> np.ndarray:
        """Extract rotation as unit quaternion (q0, q1, q2, q3) [2.3]

        Quaternions provide numerical stability over rotation matrices
        for iterative computations and avoid gimbal lock.
        """
        return R.from_matrix(self.rotation).as_quat()  # Returns [x, y, z, w]


class SE3Trajectory:
    """
    Discrete SE(3) trajectory: sequence of rigid body poses [2.1, 2.2]

    Represents a walk through SE(3) state space, modeling:
    - Agricultural cycles through fertility-state-space
    - Ecological interventions through ecosystem-health-space
    - Narrative arcs through story-space
    """

    def __init__(self, poses: List[SE3Pose], bounded: bool = True, r_max: float = 1.0):
        """
        Initialize SE(3) trajectory with optional bounding.

        Args:
            poses: Sequence of SE(3) poses
            bounded: Whether to enforce translation bounds (required for non-compact SE(3)) [3.1]
            r_max: Maximum translation radius (Euclidean norm)
        """
        self.poses = poses
        self.bounded = bounded
        self.r_max = r_max

        if bounded:
            self._validate_bounds()

    def _validate_bounds(self):
        """Ensure all translations satisfy |p| ≤ r_max [3.1]"""
        for pose in self.poses:
            norm = np.linalg.norm(pose.translation)
            assert norm <= self.r_max, \
                f"Translation norm {norm} exceeds r_max {self.r_max}"

    def __len__(self) -> int:
        return len(self.poses)

    def __getitem__(self, idx: int) -> SE3Pose:
        return self.poses[idx]


def compose_se3(pose1: SE3Pose, pose2: SE3Pose) -> SE3Pose:
    """
    Compose two SE(3) transformations: g_total = g1 * g2 [1.1]

    In homogeneous coordinates:
    [R1 p1]   [R2 p2]   [R1*R2  R1*p2 + p1]
    [0  1 ] * [0  1 ] = [0      1         ]

    Physical interpretation: Apply g2 first (local frame), then g1 (global frame)

    Args:
        pose1: First SE(3) transformation
        pose2: Second SE(3) transformation

    Returns:
        Composed SE(3) transformation
    """
    R_total = pose1.rotation @ pose2.rotation
    p_total = pose1.rotation @ pose2.translation + pose1.translation
    return SE3Pose(rotation=R_total, translation=p_total)


def compose_trajectory(trajectory: SE3Trajectory) -> SE3Pose:
    """
    Compose entire SE(3) trajectory: G = g1 * g2 * ... * gT [1.1]

    This computes the total transformation resulting from sequential
    application of all poses in the trajectory.

    Args:
        trajectory: SE(3) trajectory to compose

    Returns:
        Total SE(3) transformation
    """
    result = SE3Pose.identity()
    for pose in trajectory.poses:
        result = compose_se3(result, pose)
    return result


def scale_se3_pose(pose: SE3Pose, lambda_scale: float) -> SE3Pose:
    """
    Scale an SE(3) pose by factor λ [2.1, 2.2]

    Scaling in SE(3) requires different treatment for rotation and translation:
    - Rotation: R^λ = exp(λ * log(R)) via Lie algebra [2.3]
    - Translation: p^λ = λ * p (linear scaling)

    Physical interpretation:
    - λ < 1: Gentler intervention, longer timescales
    - λ > 1: Intensified intervention, compressed timescales
    - λ ≈ 0.618 (golden ratio): Often optimal in natural systems

    Args:
        pose: SE(3) pose to scale
        lambda_scale: Scaling factor

    Returns:
        Scaled SE(3) pose
    """
    # Scale rotation via Lie algebra [2.3]
    rot_vec = R.from_matrix(pose.rotation).as_rotvec()
    scaled_rot_vec = lambda_scale * rot_vec
    scaled_rotation = R.from_rotvec(scaled_rot_vec).as_matrix()

    # Scale translation linearly
    scaled_translation = lambda_scale * pose.translation

    return SE3Pose(rotation=scaled_rotation, translation=scaled_translation)


def scale_trajectory(trajectory: SE3Trajectory, lambda_scale: float) -> SE3Trajectory:
    """
    Scale entire trajectory: G_λ = g1^λ * g2^λ * ... * gT^λ [2.1]

    Each pose in the trajectory is scaled individually before composition.
    This is the key operation for double-and-scale return mechanism.

    Args:
        trajectory: Original SE(3) trajectory
        lambda_scale: Scaling factor

    Returns:
        Scaled SE(3) trajectory
    """
    scaled_poses = [scale_se3_pose(pose, lambda_scale) for pose in trajectory.poses]
    return SE3Trajectory(scaled_poses, trajectory.bounded, trajectory.r_max)


def double_trajectory(trajectory: SE3Trajectory) -> SE3Trajectory:
    """
    Double a trajectory: G^2 = G * G [Key principle from Eckmann & Tlusty 2025]

    This is the "doubling" operation that, combined with scaling, enables
    approximate returns to identity even though single traversals almost never return.

    Regenerative interpretation: One pass rarely restores equilibrium;
    two passes, properly scaled, achieve transformed return.

    Args:
        trajectory: SE(3) trajectory to double

    Returns:
        Doubled trajectory (concatenated with itself)
    """
    return SE3Trajectory(
        trajectory.poses + trajectory.poses,
        trajectory.bounded,
        trajectory.r_max
    )


def frobenius_distance_to_identity(pose: SE3Pose) -> float:
    """
    Compute Frobenius norm distance to identity: ||g - I||_F [2.3]

    This metric quantifies how far a pose is from the identity transformation.
    Used as the cost function for optimizing scaling factor λ.

    Distance combines:
    - Rotation error: ||R - I||_F
    - Translation error: ||p||_2

    Args:
        pose: SE(3) pose to measure

    Returns:
        Scalar distance to identity
    """
    rotation_error = np.linalg.norm(pose.rotation - np.eye(3), 'fro')
    translation_error = np.linalg.norm(pose.translation)
    return rotation_error + translation_error


def compute_return_error(
    trajectory: SE3Trajectory,
    lambda_scale: float,
    double: bool = True
) -> float:
    """
    Compute return error for scaled (and optionally doubled) trajectory [2.3]

    Error metric: ||G_λ^n - I||_F where n = 2 (doubled) or 1 (single pass)

    This is the core cost function optimized to find the scaling factor λ
    that brings the system closest to identity (return/reset).

    Args:
        trajectory: SE(3) trajectory
        lambda_scale: Scaling factor to test
        double: Whether to double the trajectory (recommended: True)

    Returns:
        Frobenius distance to identity after scaling (and doubling)
    """
    # Scale trajectory
    scaled = scale_trajectory(trajectory, lambda_scale)

    # Double if requested (key to the return mechanism)
    if double:
        scaled = double_trajectory(scaled)

    # Compose to get total transformation
    total_pose = compose_trajectory(scaled)

    # Measure distance to identity
    return frobenius_distance_to_identity(total_pose)


def optimize_scaling_factor(
    trajectory: SE3Trajectory,
    lambda_bounds: Tuple[float, float] = (0.1, 2.0),
    double: bool = True,
    method: str = 'bounded'
) -> OptimizeResult:
    """
    Find optimal scaling factor λ for approximate return to identity [2.3]

    Solves: argmin_λ ||G_λ^2 - I||_F

    The optimization searches for the scaling factor that, when applied to
    the trajectory and doubled, brings the system closest to identity.

    Physical interpretation: Finding the intervention intensity that enables
    regenerative return after two cycles.

    Args:
        trajectory: SE(3) trajectory to optimize
        lambda_bounds: Search bounds for λ (default: [0.1, 2.0])
        double: Whether to use double-and-scale (recommended: True)
        method: Scipy optimization method (default: 'bounded')

    Returns:
        Scipy optimization result with optimal λ in result.x

    Example:
        >>> trajectory = generate_random_trajectory(T=10, r_max=1.0)
        >>> result = optimize_scaling_factor(trajectory)
        >>> lambda_opt = result.x
        >>> print(f"Optimal scaling: {lambda_opt:.4f}, Error: {result.fun:.6f}")
    """
    # Define cost function
    def cost(lam: float) -> float:
        return compute_return_error(trajectory, lam, double=double)

    # Optimize using scipy
    result = minimize_scalar(
        cost,
        bounds=lambda_bounds,
        method=method
    )

    return result


class TetheredSE3Walker:
    """
    Tethered random walk in SE(3) with elastic return force [Opus insight]

    Creates artificial boundedness enabling returns without hard boundaries.
    Models systems with "memory" of home state that pull back via elastic force.

    Physical analogy: Agricultural systems tethered to baseline soil health,
    or narratives tethered to equilibrium state, experiencing "restoring force".
    """

    def __init__(
        self,
        home: SE3Pose = None,
        elastic_constant: float = 0.1,
        translation_noise: float = 0.05,
        rotation_noise: float = 0.1
    ):
        """
        Initialize tethered walker.

        Args:
            home: Tether point (default: identity)
            elastic_constant: Return force strength (k in Hooke's law)
            translation_noise: Stochastic translation amplitude
            rotation_noise: Stochastic rotation amplitude (radians)
        """
        self.home = home if home is not None else SE3Pose.identity()
        self.k = elastic_constant
        self.translation_noise = translation_noise
        self.rotation_noise = rotation_noise
        self.current_position = SE3Pose.identity()

    def compute_return_force(self) -> Tuple[np.ndarray, np.ndarray]:
        """
        Compute elastic return force toward home [Opus insight]

        Returns:
            (translation_force, rotation_force) pulling toward home
        """
        # Translation force: linear spring (Hooke's law)
        trans_force = -self.k * (
            self.current_position.translation - self.home.translation
        )

        # Rotation force: logarithmic map in Lie algebra [2.3]
        relative_rotation = self.home.rotation.T @ self.current_position.rotation
        rot_deviation = R.from_matrix(relative_rotation).as_rotvec()
        rot_force = -self.k * rot_deviation

        return trans_force, rot_force

    def step(self, dt: float = 0.1) -> SE3Pose:
        """
        Take one step in tethered random walk [5.1]

        Combines:
        - Elastic return force (deterministic)
        - Stochastic noise (random perturbation)

        Args:
            dt: Time step size

        Returns:
            New SE(3) position
        """
        # Compute return forces
        trans_force, rot_force = self.compute_return_force()

        # Add stochastic noise (Ornstein-Uhlenbeck process) [5.1]
        trans_noise = np.random.normal(0, self.translation_noise, 3)
        rot_noise = np.random.normal(0, self.rotation_noise, 3)

        # Update translation: deterministic force + noise
        new_translation = (
            self.current_position.translation +
            dt * trans_force +
            np.sqrt(dt) * trans_noise
        )

        # Update rotation: deterministic force + noise
        current_rotvec = self.current_position.to_rotation_vector()
        new_rotvec = current_rotvec + dt * rot_force + np.sqrt(dt) * rot_noise
        new_rotation = R.from_rotvec(new_rotvec).as_matrix()

        # Create new pose
        self.current_position = SE3Pose(rotation=new_rotation, translation=new_translation)
        return self.current_position


def generate_random_trajectory(
    T: int = 10,
    r_max: float = 1.0,
    rotation_scale: float = 0.1,
    bounded: bool = True
) -> SE3Trajectory:
    """
    Generate random bounded SE(3) trajectory for testing [3.1, 4.1]

    Creates a trajectory with small random rotations and translations,
    respecting the bounded domain constraint.

    Args:
        T: Number of steps in trajectory
        r_max: Maximum translation radius
        rotation_scale: Scale of random rotations (radians)
        bounded: Whether to enforce bounds

    Returns:
        Random SE(3) trajectory
    """
    poses = []
    for _ in range(T):
        # Random small rotation
        rot_vec = np.random.randn(3) * rotation_scale
        rotation = R.from_rotvec(rot_vec).as_matrix()

        # Random small translation (within bounds)
        translation = np.random.randn(3) * (r_max / T)

        poses.append(SE3Pose(rotation=rotation, translation=translation))

    return SE3Trajectory(poses, bounded=bounded, r_max=r_max)


def verify_approximate_return(
    trajectory: SE3Trajectory,
    lambda_opt: float,
    tolerance: float = 0.1,
    double: bool = True
) -> Dict[str, float]:
    """
    Verify that scaled (doubled) trajectory achieves approximate return [2.3]

    Computes various metrics to assess quality of return to identity.

    Args:
        trajectory: SE(3) trajectory
        lambda_opt: Optimized scaling factor
        tolerance: Acceptable error threshold
        double: Whether trajectory was doubled

    Returns:
        Dictionary with return quality metrics
    """
    # Compute final pose
    scaled = scale_trajectory(trajectory, lambda_opt)
    if double:
        scaled = double_trajectory(scaled)
    final_pose = compose_trajectory(scaled)

    # Compute error metrics
    total_error = frobenius_distance_to_identity(final_pose)
    rotation_error = np.linalg.norm(final_pose.rotation - np.eye(3), 'fro')
    translation_error = np.linalg.norm(final_pose.translation)

    # Assess return quality
    return_achieved = total_error < tolerance

    return {
        'total_error': total_error,
        'rotation_error': rotation_error,
        'translation_error': translation_error,
        'return_achieved': return_achieved,
        'tolerance': tolerance,
        'lambda': lambda_opt
    }


# ==================== Advanced Patterns ====================

def adjoint_action(g: SE3Pose, X: np.ndarray) -> np.ndarray:
    """
    Adjoint action: How transformation g conjugates generator X [Opus insight]

    Ad_g(X) = g X g^(-1) in matrix form

    In Lie algebra: describes how applying transformation g changes
    the effect of infinitesimal generator X.

    Application: Predict whether interventions will constructively
    or destructively interfere when composed.

    Args:
        g: SE(3) transformation
        X: Lie algebra element (6D: 3 rotation + 3 translation)

    Returns:
        Transformed Lie algebra element
    """
    # For SO(3), adjoint is simply R X R^T for rotation part
    # This is a simplified version; full SE(3) adjoint is more complex
    rotation_part = g.rotation @ X[:3, :3] @ g.rotation.T

    # Translation part transformation
    translation_part = g.rotation @ X[:3, 3]

    # Reconstruct
    result = np.zeros((4, 4))
    result[:3, :3] = rotation_part
    result[:3, 3] = translation_part

    return result


def predict_intervention_interference(
    intervention1: SE3Pose,
    intervention2: SE3Pose
) -> float:
    """
    Predict interference between two interventions [Opus insight]

    Quantifies how much applying intervention1 changes the effect
    of intervention2 via the adjoint action.

    Args:
        intervention1: First SE(3) transformation
        intervention2: Second SE(3) transformation

    Returns:
        Interference magnitude (0 = no interference, >0 = interference)
    """
    # Convert intervention2 to Lie algebra element
    X = np.eye(4)
    X[:3, :3] = intervention2.rotation
    X[:3, 3] = intervention2.translation

    # Apply adjoint action
    transformed = adjoint_action(intervention1, X)

    # Measure difference
    interference = np.linalg.norm(transformed - X, 'fro')

    return interference
