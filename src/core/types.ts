/**
 * TypeScript Types for SE(3) Regenerative Metrics Service
 *
 * PROVENANCE:
 * -----------
 * Created for: Open Science DLT - Pillar I (Science)
 * Integration Date: 2025-11-11
 * Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS
 * Purpose: Type definitions for Python→TypeScript integration
 *
 * These types match the Python API contract for the SE(3) λ-estimation service.
 */

/**
 * SE(3) Pose representation
 *
 * rotation: 3D rotation vector (axis-angle) or 3x3 rotation matrix
 * translation: 3D translation vector
 */
export interface SE3Pose {
  rotation: number[] | number[][];
  translation: number[];
}

/**
 * Trajectory data input formats
 */
export interface TrajectoryDataPoses {
  poses: SE3Pose[];
}

export interface TrajectoryDataTimeSeries {
  positions: number[][];
  orientations: number[][];
}

export interface TrajectoryDataStateVectors {
  state_vectors: number[][];
}

export type TrajectoryData =
  | TrajectoryDataPoses
  | TrajectoryDataTimeSeries
  | TrajectoryDataStateVectors;

/**
 * Computation options
 */
export interface MetricsComputationOptions {
  enable_resonance_detection?: boolean;
  enable_verification_cascade?: boolean;
  bounded?: boolean;
  r_max?: number;
  lambda_bounds?: [number, number];
}

/**
 * Regenerative metrics output
 *
 * Core metrics as specified in UCF directive:
 * - optimal_lambda: Optimal scaling factor λ
 * - return_error_epsilon: Return error ε to identity
 * - verification_score: Multi-level verification score [0, 1]
 */
export interface RegenerativeMetrics {
  optimal_lambda: number;
  return_error_epsilon: number;
  verification_score: number;
  resonance_detected?: string;
  confidence: number;
  metadata: {
    trajectory_length: number;
    bounded: boolean;
    r_max: number;
    lambda_bounds: [number, number];
    optimization_success: boolean;
    optimization_iterations?: number;
    timestamp: string;
  };
}

/**
 * API request payload
 */
export interface MetricsRequest {
  trajectory_data: TrajectoryData;
  options?: MetricsComputationOptions;
}

/**
 * API response
 */
export interface MetricsResponse {
  status: 'success' | 'error' | 'validation_error';
  data?: RegenerativeMetrics;
  error?: string;
  message?: string;
}

/**
 * Batch request payload
 */
export interface BatchMetricsRequest {
  trajectories: TrajectoryData[];
  options?: MetricsComputationOptions;
}

/**
 * Batch response
 */
export interface BatchMetricsResponse {
  status: 'success' | 'error';
  data?: RegenerativeMetrics[];
  count?: number;
  error?: string;
  message?: string;
}

/**
 * Health check response
 */
export interface HealthCheckResponse {
  status: 'ok' | 'error';
  service: string;
  version: string;
  timestamp: string;
}

/**
 * Version info response
 */
export interface VersionInfoResponse {
  service: string;
  version: string;
  provenance: {
    source: string;
    repository: string;
    integration_date: string;
    session: string;
  };
  mathematical_foundation: {
    principle: string;
    reference: string;
    adr: string;
  };
}

/**
 * Service configuration
 */
export interface ScienceServiceConfig {
  baseUrl: string;
  timeout?: number;
  retries?: number;
}

/**
 * Error types
 */
export class ScienceServiceError extends Error {
  constructor(
    message: string,
    public statusCode?: number,
    public details?: any
  ) {
    super(message);
    this.name = 'ScienceServiceError';
  }
}

export class ValidationError extends ScienceServiceError {
  constructor(message: string, details?: any) {
    super(message, 400, details);
    this.name = 'ValidationError';
  }
}

export class ServiceUnavailableError extends ScienceServiceError {
  constructor(message: string, details?: any) {
    super(message, 503, details);
    this.name = 'ServiceUnavailableError';
  }
}
