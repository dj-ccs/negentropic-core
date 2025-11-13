"""
Example Usage of SE(3) Regenerative Metrics Service

Demonstrates how to use the λ-estimation service for various applications.
"""

import numpy as np
from lie_dynamics.metrics_service import compute_regenerative_metrics


def example_1_basic_usage():
    """Example 1: Basic trajectory metrics computation"""
    print("=" * 60)
    print("EXAMPLE 1: Basic Trajectory Metrics")
    print("=" * 60)

    # Define a simple trajectory (rotation vectors + translations)
    trajectory_data = {
        "poses": [
            {"rotation": [0.1, 0, 0], "translation": [0.5, 0, 0]},
            {"rotation": [0, 0.1, 0], "translation": [0, 0.5, 0]},
            {"rotation": [-0.1, 0, 0], "translation": [-0.5, 0, 0]},
            {"rotation": [0, -0.1, 0], "translation": [0, -0.5, 0]},
        ]
    }

    # Compute regenerative metrics
    metrics = compute_regenerative_metrics(trajectory_data)

    print(f"\nResults:")
    print(f"  Optimal λ: {metrics['optimal_lambda']:.4f}")
    print(f"  Return Error ε: {metrics['return_error_epsilon']:.6f}")
    print(f"  Verification Score: {metrics['verification_score']:.4f}")
    print(f"  Confidence: {metrics['confidence']:.4f}")

    if metrics['resonance_detected']:
        print(f"  Resonance Detected: {metrics['resonance_detected']}")

    print(f"\nMetadata:")
    print(f"  Trajectory Length: {metrics['metadata']['trajectory_length']}")
    print(f"  Optimization Success: {metrics['metadata']['optimization_success']}")
    print()


def example_2_agricultural_rotation():
    """Example 2: Agricultural rotation cycle (hemp → wheat)"""
    print("=" * 60)
    print("EXAMPLE 2: Agricultural Rotation Cycle")
    print("=" * 60)

    # Hemp intervention: changes soil structure + nutrients
    hemp_rotation = {"rotation": [0.3, 0.1, 0.0], "translation": [0.5, 0.0, 0.0]}

    # Wheat intervention: different soil transformation
    wheat_rotation = {"rotation": [-0.2, 0.15, 0.0], "translation": [0.0, 0.3, 0.0]}

    # Rotation cycle: hemp → wheat → hemp → wheat (doubled via optimization)
    trajectory_data = {
        "poses": [hemp_rotation, wheat_rotation]
    }

    print("\nComputing optimal intervention intensity for crop rotation...")

    metrics = compute_regenerative_metrics(
        trajectory_data,
        enable_resonance_detection=True,
        enable_verification_cascade=True,
        r_max=2.0  # Larger bounds for agricultural systems
    )

    print(f"\nOptimal Intervention Intensity: λ = {metrics['optimal_lambda']:.4f}")
    print(f"Soil Health Return Quality: ε = {metrics['return_error_epsilon']:.6f}")
    print(f"System Resilience Score: {metrics['verification_score']:.4f}")

    # Interpretation
    if metrics['optimal_lambda'] < 0.7:
        print("\n→ Recommendation: Gentle interventions, longer cycle duration")
    elif metrics['optimal_lambda'] < 1.0:
        print("\n→ Recommendation: Moderate interventions, standard timing")
    else:
        print("\n→ Recommendation: Intensified interventions, compressed cycles")

    print()


def example_3_sensor_calibration():
    """Example 3: Digital twin sensor calibration"""
    print("=" * 60)
    print("EXAMPLE 3: Sensor Network Calibration")
    print("=" * 60)

    # Simulate sensor drift over time (position + orientation)
    np.random.seed(42)

    # Position drift (3D coordinates)
    positions = np.array([
        [0.0, 0.0, 0.0],
        [0.01, 0.01, 0.0],
        [0.02, 0.02, 0.01],
        [0.03, 0.025, 0.01],
        [0.035, 0.03, 0.015]
    ])

    # Orientation drift (rotation vectors)
    orientations = np.array([
        [0.0, 0.0, 0.0],
        [0.01, 0.0, 0.0],
        [0.015, 0.01, 0.0],
        [0.02, 0.015, 0.0],
        [0.025, 0.02, 0.005]
    ])

    trajectory_data = {
        "positions": positions.tolist(),
        "orientations": orientations.tolist()
    }

    print("\nComputing optimal sensor sampling rate from drift patterns...")

    metrics = compute_regenerative_metrics(
        trajectory_data,
        bounded=True,
        r_max=1.0,
        lambda_bounds=(0.1, 2.0)
    )

    print(f"\nOptimal Temporal Scaling: λ = {metrics['optimal_lambda']:.4f}")
    print(f"Calibration Closure Error: ε = {metrics['return_error_epsilon']:.6f}")
    print(f"Verification Quality: {metrics['verification_score']:.4f}")

    # Compute suggested sampling interval
    base_interval_seconds = 60  # 1 minute baseline
    optimal_interval = base_interval_seconds * metrics['optimal_lambda']

    print(f"\n→ Recommended Sampling Interval: {optimal_interval:.1f} seconds")
    print()


def example_4_resonance_exploration():
    """Example 4: Explore resonance preferences"""
    print("=" * 60)
    print("EXAMPLE 4: Resonance Detection Exploration")
    print("=" * 60)

    np.random.seed(123)

    # Generate multiple random trajectories
    num_trials = 5
    lambdas = []
    resonances = []

    print("\nGenerating random trajectories and detecting resonances...\n")

    for i in range(num_trials):
        # Random trajectory
        poses = []
        for _ in range(8):
            rot = (np.random.randn(3) * 0.1).tolist()
            trans = (np.random.randn(3) * 0.2).tolist()
            poses.append({"rotation": rot, "translation": trans})

        trajectory_data = {"poses": poses}

        metrics = compute_regenerative_metrics(
            trajectory_data,
            enable_resonance_detection=True
        )

        lambdas.append(metrics['optimal_lambda'])
        resonances.append(metrics.get('resonance_detected', 'none'))

        print(f"Trial {i+1}: λ = {metrics['optimal_lambda']:.4f}, "
              f"Resonance: {metrics.get('resonance_detected', 'none')}")

    # Summary statistics
    golden_ratio = (np.sqrt(5) - 1) / 2
    near_golden = sum(1 for lam in lambdas if abs(lam - golden_ratio) / golden_ratio < 0.3)

    print(f"\n--- Summary ---")
    print(f"Golden Ratio (φ): {golden_ratio:.4f}")
    print(f"Mean λ: {np.mean(lambdas):.4f}")
    print(f"Std λ: {np.std(lambdas):.4f}")
    print(f"Near Golden Ratio: {near_golden}/{num_trials} trials")
    print(f"Resonances Detected: {set(resonances)}")
    print()


def example_5_batch_processing():
    """Example 5: Batch processing multiple trajectories"""
    print("=" * 60)
    print("EXAMPLE 5: Batch Processing")
    print("=" * 60)

    from lie_dynamics.metrics_service import compute_batch_metrics

    # Multiple trajectories
    trajectories = [
        {
            "poses": [
                {"rotation": [0.1, 0, 0], "translation": [0.2, 0, 0]},
                {"rotation": [0, 0.1, 0], "translation": [0, 0.2, 0]},
            ]
        },
        {
            "poses": [
                {"rotation": [0.2, 0.1, 0], "translation": [0.3, 0.1, 0]},
                {"rotation": [0.1, 0.2, 0], "translation": [0.1, 0.3, 0]},
            ]
        },
        {
            "poses": [
                {"rotation": [0.05, 0, 0], "translation": [0.1, 0, 0]},
                {"rotation": [0, 0.05, 0], "translation": [0, 0.1, 0]},
            ]
        }
    ]

    print(f"\nProcessing {len(trajectories)} trajectories in batch...")

    results = compute_batch_metrics(
        trajectories,
        enable_resonance_detection=False,  # Faster for batch
        enable_verification_cascade=True
    )

    print(f"\nResults:")
    for i, metrics in enumerate(results):
        if "error" not in metrics:
            print(f"  Trajectory {i+1}: λ = {metrics['optimal_lambda']:.4f}, "
                  f"Score = {metrics['verification_score']:.4f}")
        else:
            print(f"  Trajectory {i+1}: ERROR - {metrics['error']}")

    print()


if __name__ == "__main__":
    print("\n" + "=" * 60)
    print("SE(3) Regenerative Metrics Service - Usage Examples")
    print("=" * 60 + "\n")

    # Run all examples
    example_1_basic_usage()
    example_2_agricultural_rotation()
    example_3_sensor_calibration()
    example_4_resonance_exploration()
    example_5_batch_processing()

    print("=" * 60)
    print("All examples completed successfully!")
    print("=" * 60)
