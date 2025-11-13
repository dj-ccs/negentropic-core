/**
 * TypeScript Client for SE(3) Regenerative Metrics Service
 *
 * PROVENANCE:
 * -----------
 * Created for: Open Science DLT - Pillar I (Science)
 * Integration Date: 2025-11-11
 * Session: claude/se3-lambda-estimation-service-011CV1jDenzxAxddVhcat8uS
 * Purpose: TypeScript client for Python SE(3) service integration
 *
 * This client provides a type-safe wrapper around the Python Flask API,
 * enabling seamless integration of UCF SE(3) mathematics into the
 * TypeScript/Node.js Open Science DLT platform.
 */

import axios, { AxiosInstance, AxiosError } from 'axios';
import {
  ScienceServiceConfig,
  TrajectoryData,
  MetricsComputationOptions,
  RegenerativeMetrics,
  MetricsRequest,
  MetricsResponse,
  BatchMetricsRequest,
  BatchMetricsResponse,
  HealthCheckResponse,
  VersionInfoResponse,
  ScienceServiceError,
  ValidationError,
  ServiceUnavailableError,
} from './types';

/**
 * Client for SE(3) Regenerative Metrics Service
 *
 * Usage:
 * ```typescript
 * const client = new ScienceServiceClient({
 *   baseUrl: 'http://localhost:5000'
 * });
 *
 * const metrics = await client.computeMetrics({
 *   trajectory_data: {
 *     poses: [
 *       { rotation: [0.1, 0, 0], translation: [0.5, 0, 0] },
 *       { rotation: [0, 0.1, 0], translation: [0, 0.5, 0] }
 *     ]
 *   }
 * });
 *
 * console.log(`Optimal λ: ${metrics.optimal_lambda}`);
 * ```
 */
export class ScienceServiceClient {
  private client: AxiosInstance;
  private baseUrl: string;

  constructor(config: ScienceServiceConfig) {
    this.baseUrl = config.baseUrl;

    this.client = axios.create({
      baseURL: config.baseUrl,
      timeout: config.timeout || 30000, // 30s default
      headers: {
        'Content-Type': 'application/json',
      },
    });

    // Setup interceptors for error handling
    this.client.interceptors.response.use(
      (response) => response,
      (error: AxiosError) => {
        return Promise.reject(this.handleError(error));
      }
    );
  }

  /**
   * Handle API errors and convert to typed exceptions
   */
  private handleError(error: AxiosError): ScienceServiceError {
    if (error.response) {
      // Server responded with error
      const status = error.response.status;
      const data: any = error.response.data;

      if (status === 400) {
        return new ValidationError(
          data.error || 'Validation failed',
          data.message
        );
      } else if (status === 503) {
        return new ServiceUnavailableError(
          'Science service unavailable',
          data.message
        );
      } else {
        return new ScienceServiceError(
          data.error || 'Unknown error',
          status,
          data.message
        );
      }
    } else if (error.request) {
      // No response received
      return new ServiceUnavailableError(
        'No response from science service',
        error.message
      );
    } else {
      // Request setup error
      return new ScienceServiceError(error.message);
    }
  }

  /**
   * Compute regenerative metrics for a single trajectory
   *
   * @param trajectoryData - Input trajectory data
   * @param options - Computation options
   * @returns Regenerative metrics
   *
   * @example
   * ```typescript
   * const metrics = await client.computeMetrics({
   *   trajectory_data: { poses: [...] },
   *   options: {
   *     enable_resonance_detection: true,
   *     lambda_bounds: [0.1, 2.0]
   *   }
   * });
   * ```
   */
  async computeMetrics(
    trajectoryData: TrajectoryData,
    options?: MetricsComputationOptions
  ): Promise<RegenerativeMetrics> {
    const request: MetricsRequest = {
      trajectory_data: trajectoryData,
      options: options,
    };

    const response = await this.client.post<MetricsResponse>(
      '/api/v1/science/metrics',
      request
    );

    if (response.data.status !== 'success' || !response.data.data) {
      throw new ScienceServiceError(
        response.data.error || 'Metrics computation failed',
        response.status
      );
    }

    return response.data.data;
  }

  /**
   * Compute regenerative metrics for multiple trajectories in batch
   *
   * @param trajectories - Array of trajectory data
   * @param options - Computation options
   * @returns Array of regenerative metrics
   *
   * @example
   * ```typescript
   * const results = await client.computeMetricsBatch([
   *   { poses: [...] },
   *   { poses: [...] }
   * ]);
   * ```
   */
  async computeMetricsBatch(
    trajectories: TrajectoryData[],
    options?: MetricsComputationOptions
  ): Promise<RegenerativeMetrics[]> {
    const request: BatchMetricsRequest = {
      trajectories: trajectories,
      options: options,
    };

    const response = await this.client.post<BatchMetricsResponse>(
      '/api/v1/science/metrics/batch',
      request
    );

    if (response.data.status !== 'success' || !response.data.data) {
      throw new ScienceServiceError(
        response.data.error || 'Batch metrics computation failed',
        response.status
      );
    }

    return response.data.data;
  }

  /**
   * Check health status of the science service
   *
   * @returns Health check response
   */
  async healthCheck(): Promise<HealthCheckResponse> {
    const response = await this.client.get<HealthCheckResponse>(
      '/api/v1/science/health'
    );

    return response.data;
  }

  /**
   * Get version information about the science service
   *
   * @returns Version info response
   */
  async getVersion(): Promise<VersionInfoResponse> {
    const response = await this.client.get<VersionInfoResponse>(
      '/api/v1/science/version'
    );

    return response.data;
  }

  /**
   * Check if the science service is available
   *
   * @returns True if service is healthy, false otherwise
   */
  async isAvailable(): Promise<boolean> {
    try {
      const health = await this.healthCheck();
      return health.status === 'ok';
    } catch (error) {
      return false;
    }
  }
}

/**
 * Factory function to create a science service client
 *
 * @param config - Service configuration
 * @returns Configured client instance
 */
export function createScienceServiceClient(
  config?: Partial<ScienceServiceConfig>
): ScienceServiceClient {
  const defaultConfig: ScienceServiceConfig = {
    baseUrl: process.env.SCIENCE_SERVICE_URL || 'http://localhost:5000',
    timeout: 30000,
  };

  return new ScienceServiceClient({ ...defaultConfig, ...config });
}

/**
 * Singleton instance for default configuration
 * Usage: import { scienceClient } from './science/client';
 */
export const scienceClient = createScienceServiceClient();
