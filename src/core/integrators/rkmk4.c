// rkmk4.c - Runge-Kutta-Munthe-Kaas Integrator for SE(3)
//
// 4th-order geometric integrator for Lie groups (SE(3) poses).
// Preserves group structure (rotation orthogonality) exactly.
//
// Algorithm:
//   1. Compute Lie algebra stages (twists) using BCH truncation
//   2. Map to group via exponential map (LUT-accelerated)
//   3. Compose pose: g ← g * exp(ξ_dt)
//   4. Re-orthonormalize rotation if needed
//
// Reference: docs/integrators.md section 3.2
// Based on: embedded/se3_math.c
// Author: negentropic-core team
// Version: 2.2.0

#include "integrators.h"
#include "workspace.h"
#include <math.h>
#include <string.h>

/* ========================================================================
 * SE(3) HELPERS
 * ======================================================================== */

/**
 * SE(3) pose structure (internal, simplified).
 *
 * Full implementation should use se3_pose_t from se3_edge.h,
 * but for now we use a simplified version.
 */
typedef struct {
    double R[9];           // 3x3 rotation matrix (row-major)
    double t[3];           // Translation vector
} SE3Pose;

/**
 * Lie algebra twist (se(3) element).
 *
 * 6-dimensional vector: [ω, v]
 *   ω: angular velocity (3D)
 *   v: linear velocity (3D)
 */
typedef struct {
    double omega[3];       // Angular velocity (rotation axis × magnitude)
    double v[3];           // Linear velocity
} SE3Twist;

/* ========================================================================
 * EXPONENTIAL MAP (LUT-ACCELERATED)
 * ======================================================================== */

/**
 * Exponential map: se(3) → SE(3)
 *
 * Maps Lie algebra element (twist) to group element (pose).
 *
 * Algorithm (Rodriguez formula for rotation):
 *   R = I + (sin θ / θ) [ω]× + ((1 - cos θ) / θ²) [ω]×²
 *
 * where θ = |ω| and [ω]× is the skew-symmetric matrix.
 *
 * For small angles (θ < 0.01), use Taylor series.
 *
 * @param twist Input twist (Lie algebra element)
 * @param dt Timestep scaling
 * @param ws Workspace (for LUT access)
 * @param pose Output pose (SE(3) group element)
 */
static void exp_map(const SE3Twist* twist, double dt, IntegratorWorkspace* ws, SE3Pose* pose) {
    (void)ws;  // LUT not yet implemented

    // Scale twist by dt
    double omega[3] = {
        twist->omega[0] * dt,
        twist->omega[1] * dt,
        twist->omega[2] * dt
    };
    double v_scaled[3] = {
        twist->v[0] * dt,
        twist->v[1] * dt,
        twist->v[2] * dt
    };

    // Compute rotation angle
    double theta = sqrt(omega[0]*omega[0] + omega[1]*omega[1] + omega[2]*omega[2]);

    // Identity rotation
    double R[9] = {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };

    if (theta > 1e-8) {
        // Rodriguez formula
        double s = sin(theta) / theta;
        double c = (1.0 - cos(theta)) / (theta * theta);

        // Skew-symmetric matrix [ω]×
        double wx = omega[0];
        double wy = omega[1];
        double wz = omega[2];

        // [ω]× matrix
        double W[9] = {
            0,    -wz,   wy,
            wz,    0,   -wx,
            -wy,   wx,    0
        };

        // [ω]×² matrix
        double W2[9];
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                W2[i*3 + j] = 0.0;
                for (int k = 0; k < 3; k++) {
                    W2[i*3 + j] += W[i*3 + k] * W[k*3 + j];
                }
            }
        }

        // R = I + s [ω]× + c [ω]×²
        for (int i = 0; i < 9; i++) {
            R[i] += s * W[i] + c * W2[i];
        }
    }

    // Set pose
    memcpy(pose->R, R, sizeof(R));
    memcpy(pose->t, v_scaled, sizeof(v_scaled));
}

/* ========================================================================
 * ROTATION RE-ORTHONORMALIZATION
 * ======================================================================== */

/**
 * Re-orthonormalize rotation matrix using Gram-Schmidt.
 *
 * Ensures R remains in SO(3) despite numerical errors.
 *
 * @param R 3x3 rotation matrix (row-major, modified in-place)
 */
static void reorthonormalize_rotation(double R[9]) {
    // Extract column vectors
    double c0[3] = { R[0], R[3], R[6] };
    double c1[3] = { R[1], R[4], R[7] };
    double c2[3] = { R[2], R[5], R[8] };

    // Gram-Schmidt on columns
    // c0' = c0 / |c0|
    double norm0 = sqrt(c0[0]*c0[0] + c0[1]*c0[1] + c0[2]*c0[2]);
    c0[0] /= norm0; c0[1] /= norm0; c0[2] /= norm0;

    // c1' = c1 - (c1·c0')c0'
    double dot01 = c1[0]*c0[0] + c1[1]*c0[1] + c1[2]*c0[2];
    c1[0] -= dot01 * c0[0];
    c1[1] -= dot01 * c0[1];
    c1[2] -= dot01 * c0[2];
    double norm1 = sqrt(c1[0]*c1[0] + c1[1]*c1[1] + c1[2]*c1[2]);
    c1[0] /= norm1; c1[1] /= norm1; c1[2] /= norm1;

    // c2' = c0' × c1' (cross product for right-handed system)
    c2[0] = c0[1]*c1[2] - c0[2]*c1[1];
    c2[1] = c0[2]*c1[0] - c0[0]*c1[2];
    c2[2] = c0[0]*c1[1] - c0[1]*c1[0];

    // Write back
    R[0] = c0[0]; R[3] = c0[1]; R[6] = c0[2];
    R[1] = c1[0]; R[4] = c1[1]; R[7] = c1[2];
    R[2] = c2[0]; R[5] = c2[1]; R[8] = c2[2];
}

/* ========================================================================
 * RKMK4 MAIN ALGORITHM
 * ======================================================================== */

/**
 * RKMK4 integration step for SE(3) pose.
 *
 * 4th-order geometric integrator:
 *   k1 = f(t, g)
 *   k2 = f(t + dt/2, g * exp(dt/2 * k1))
 *   k3 = f(t + dt/2, g * exp(dt/2 * k2))
 *   k4 = f(t + dt, g * exp(dt * k3))
 *   g_new = g * exp(dt/6 * (k1 + 2k2 + 2k3 + k4))
 *
 * @param pose Current pose (modified in-place)
 * @param twist_rate Rate of change function (for now, constant)
 * @param dt Timestep
 * @param ws Workspace
 * @return 0 on success, -1 on error
 */
static int rkmk4_step_se3(SE3Pose* pose, const SE3Twist* twist_rate, double dt, IntegratorWorkspace* ws) {
    if (!pose || !twist_rate || !ws) return -1;

    // For now, we use a simplified constant-twist model
    // Full implementation would evaluate twist_rate at intermediate stages

    // k1 = f(t, g) = twist_rate
    memcpy(ws->k1, twist_rate, sizeof(SE3Twist));

    // k2 = f(t + dt/2, g * exp(dt/2 * k1))
    // Simplified: k2 ≈ k1 (constant twist approximation)
    memcpy(ws->k2, twist_rate, sizeof(SE3Twist));

    // k3 = f(t + dt/2, g * exp(dt/2 * k2))
    memcpy(ws->k3, twist_rate, sizeof(SE3Twist));

    // k4 = f(t + dt, g * exp(dt * k3))
    memcpy(ws->k4, twist_rate, sizeof(SE3Twist));

    // Combine stages: ξ_combined = dt/6 * (k1 + 2k2 + 2k3 + k4)
    SE3Twist combined;
    for (int i = 0; i < 3; i++) {
        combined.omega[i] = (dt / 6.0) * (
            ws->k1[i] + 2.0 * ws->k2[i] + 2.0 * ws->k3[i] + ws->k4[i]
        );
        combined.v[i] = (dt / 6.0) * (
            ws->k1[i+3] + 2.0 * ws->k2[i+3] + 2.0 * ws->k3[i+3] + ws->k4[i+3]
        );
    }

    // Apply exponential map: exp(ξ_combined)
    SE3Pose delta_pose;
    exp_map(&combined, 1.0, ws, &delta_pose);  // dt already included in combined

    // Compose: g_new = g * delta_pose
    // For simplicity, we just update rotation and translation directly
    // Full implementation would do proper SE(3) composition

    // Update rotation: R_new = R * R_delta
    double R_new[9];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            R_new[i*3 + j] = 0.0;
            for (int k = 0; k < 3; k++) {
                R_new[i*3 + j] += pose->R[i*3 + k] * delta_pose.R[k*3 + j];
            }
        }
    }
    memcpy(pose->R, R_new, sizeof(R_new));

    // Update translation: t_new = t + R * t_delta
    double t_delta_rotated[3];
    for (int i = 0; i < 3; i++) {
        t_delta_rotated[i] = 0.0;
        for (int j = 0; j < 3; j++) {
            t_delta_rotated[i] += pose->R[i*3 + j] * delta_pose.t[j];
        }
        pose->t[i] += t_delta_rotated[i];
    }

    // Re-orthonormalize rotation to maintain SO(3) property
    reorthonormalize_rotation(pose->R);

    return 0;
}

/* ========================================================================
 * INTEGRATOR INTERFACE
 * ======================================================================== */

/**
 * Integrate a grid cell using RKMK4.
 *
 * Currently a stub - needs to extract SE(3) pose from cell,
 * integrate, and write back.
 *
 * @param cell Grid cell (contains pose implicitly)
 * @param cfg Integration configuration
 * @param ws Workspace
 * @return 0 on success, error code on failure
 */
int rkmk4_integrate_cell(GridCell* cell, const IntegratorConfig* cfg, IntegratorWorkspace* ws) {
    if (!cell || !cfg || !ws) return -1;

    // TODO: Extract SE(3) pose from cell
    // For now, this is a stub that does nothing

    // Placeholder: identity pose
    SE3Pose pose;
    memset(&pose, 0, sizeof(pose));
    pose.R[0] = 1.0; pose.R[4] = 1.0; pose.R[8] = 1.0;

    // Placeholder: zero twist
    SE3Twist twist_rate;
    memset(&twist_rate, 0, sizeof(twist_rate));

    // Integrate
    int result = rkmk4_step_se3(&pose, &twist_rate, cfg->dt, ws);

    // TODO: Write pose back to cell

    return result;
}
