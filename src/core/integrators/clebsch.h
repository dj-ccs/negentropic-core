// clebsch.h - Clebsch Canonization API
//
// Lie-Poisson → Canonical Hamiltonian transformation for vorticity dynamics.
// Implements lift/project with LUT acceleration and symplectic integration.
//
// LOCKED DECISIONS (v2.2):
//   - LUT size: 512 bins (DECISION #1)
//   - Internal precision: FP64 (DECISION #2)
//   - Fallback: Single explicit Euler + Casimir correction (DECISION #5)
//
// Reference: docs/integrators.md section 4.3
// Author: negentropic-core team
// Version: 2.2.0

#ifndef NEG_CLEBSCH_H
#define NEG_CLEBSCH_H

#include "../state.h"
#include "workspace.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CLEBSCH LUT SPECIFICATION
 * ======================================================================== */

#define CLEBSCH_LUT_SIZE 512  // LOCKED DECISION #1: 512 bins (8 KB total)

/**
 * Clebsch lookup table structure.
 *
 * Precomputed for discrete Lie-Poisson bracket on cubed-sphere grid.
 * Generated offline with SymPy symbolic solver.
 *
 * Memory: 512 bins × (8 doubles q + 8 doubles p + 1 double Casimir) = ~36 KB
 */
typedef struct {
    uint16_t num_bins;        // Always 512
    double* q_table;          // Canonical position LUT [512][8]
    double* p_table;          // Canonical momentum LUT [512][8]
    double* casimir_table;    // Expected Casimir values [512]
    double vorticity_max;     // Maximum vorticity for binning
    bool initialized;         // LUT loaded successfully
} ClebschLUT;

/* ========================================================================
 * LIE-POISSON VARIABLE
 * ======================================================================== */

/**
 * Lie-Poisson variable (vorticity-like).
 *
 * For 2.5D simulation, this is primarily vertical vorticity ω_z.
 * Full 3D would have 3 components.
 */
typedef struct {
    double omega_z;           // Vertical vorticity (primary for 2.5D)
    double omega_x;           // X-component (zero for 2.5D)
    double omega_y;           // Y-component (zero for 2.5D)
    double magnitude;         // Cached magnitude |ω|
} LPVar;

/* ========================================================================
 * CLEBSCH WORKSPACE
 * ======================================================================== */

/**
 * Clebsch-specific workspace (opaque).
 *
 * Embedded in IntegratorWorkspace, accessed via opaque pointer.
 */
typedef struct ClebschWorkspace ClebschWorkspace;

/**
 * Initialize Clebsch workspace.
 *
 * @param lut Clebsch LUT (must be initialized)
 * @return Workspace pointer, or NULL on failure
 */
ClebschWorkspace* clebsch_workspace_create(const ClebschLUT* lut);

/**
 * Destroy Clebsch workspace.
 *
 * @param ws Workspace to destroy (can be NULL)
 */
void clebsch_workspace_destroy(ClebschWorkspace* ws);

/* ========================================================================
 * LIFT & PROJECT
 * ======================================================================== */

/**
 * Lift: Lie-Poisson → Canonical Hamiltonian (m → (q,p)).
 *
 * Uses LUT-accelerated lookup with linear interpolation between bins.
 *
 * Algorithm:
 *   1. Compute vorticity magnitude |ω|
 *   2. Find bin: bin = (|ω| / ω_max) * 511
 *   3. Interpolate q and p from LUT
 *
 * PRECISION: FP64 throughout (LOCKED DECISION #2)
 *
 * @param m Lie-Poisson variable (input)
 * @param ws Clebsch workspace (must be initialized)
 * @param q_out Canonical position (output, 8 doubles)
 * @param p_out Canonical momentum (output, 8 doubles)
 * @return 0 on success, -1 on error
 */
int clebsch_lift(const LPVar* m, ClebschWorkspace* ws, double* q_out, double* p_out);

/**
 * Project: Canonical Hamiltonian → Lie-Poisson ((q,p) → m).
 *
 * Inverse transformation with Casimir enforcement.
 *
 * Algorithm:
 *   1. Compute m' = J(q, p) via LUT inverse lookup
 *   2. Compute Casimir error: ΔC = C(m') - C_expected
 *   3. Apply correction if |ΔC| > tolerance
 *
 * PRECISION: FP64 throughout (LOCKED DECISION #2)
 *
 * @param q Canonical position (input, 8 doubles)
 * @param p Canonical momentum (input, 8 doubles)
 * @param ws Clebsch workspace (must be initialized)
 * @param m_out Lie-Poisson variable (output)
 * @return 0 on success, -1 on error
 */
int clebsch_project(const double* q, const double* p, ClebschWorkspace* ws, LPVar* m_out);

/* ========================================================================
 * SYMPLECTIC INTEGRATION
 * ======================================================================== */

/**
 * One symplectic RK step on canonical variables (q, p).
 *
 * Uses 2-stage partitioned Runge-Kutta (velocity-Verlet style).
 * Bounded Newton iteration for implicit stages.
 *
 * Algorithm:
 *   1. k1 = f_q(q, p)
 *   2. k2 = f_p(q + dt/2 * k1, p)  [implicit, Newton iteration]
 *   3. q' = q + dt * k2
 *   4. p' = p + dt/2 * (f_p(q, p) + f_p(q', p'))
 *
 * Fallback (LOCKED DECISION #5):
 *   If Newton iteration exceeds max_iter:
 *     1. Perform single explicit symplectic Euler step
 *     2. Call casimir_correction_sweep(q', p')
 *
 * PRECISION: FP64 throughout (LOCKED DECISION #2)
 *
 * @param q Canonical position (modified in-place, 8 doubles)
 * @param p Canonical momentum (modified in-place, 8 doubles)
 * @param dt Timestep
 * @param cfg Integrator configuration (max_iter, tolerance)
 * @param ws Clebsch workspace
 * @return 0 on success, 1 if fallback used, -1 on error
 */
int clebsch_symplectic_step(double* q, double* p, double dt,
                             const IntegratorConfig* cfg,
                             ClebschWorkspace* ws);

/* ========================================================================
 * CASIMIR ENFORCEMENT
 * ======================================================================== */

/**
 * Enforce Casimir invariants via correction sweep.
 *
 * Called automatically during projection, or explicitly after fallback.
 *
 * Algorithm:
 *   1. Compute current Casimir C(q, p)
 *   2. Compute error: ΔC = C - C_initial
 *   3. If |ΔC| > tolerance, apply scalar correction to q
 *
 * Target: |ΔC| < 1e-6 (LOCKED DECISION #2: FP64 precision)
 *
 * @param q Canonical position (modified in-place)
 * @param p Canonical momentum (not modified)
 * @param ws Clebsch workspace (contains C_initial)
 * @return Casimir error after correction
 */
double casimir_correction_sweep(double* q, const double* p, ClebschWorkspace* ws);

/**
 * Compute Casimir invariant for canonical variables.
 *
 * Casimir for discrete Lie-Poisson bracket (grid-dependent).
 *
 * @param q Canonical position (8 doubles)
 * @param p Canonical momentum (8 doubles)
 * @return Casimir value
 */
double compute_casimir(const double* q, const double* p);

/* ========================================================================
 * LUT INITIALIZATION
 * ======================================================================== */

/**
 * Initialize Clebsch LUT from precomputed data.
 *
 * LUT must be generated offline with:
 *   tools/generate_clebsch_lut.py
 *
 * @param lut LUT structure to initialize (must be allocated)
 * @return 0 on success, -1 on error
 */
int clebsch_lut_init(ClebschLUT* lut);

/**
 * Destroy Clebsch LUT and free memory.
 *
 * @param lut LUT to destroy (can be NULL)
 */
void clebsch_lut_destroy(ClebschLUT* lut);

/**
 * Load Clebsch LUT from file (binary format).
 *
 * File format:
 *   uint16_t num_bins (always 512)
 *   double vorticity_max
 *   double q_table[512][8]
 *   double p_table[512][8]
 *   double casimir_table[512]
 *
 * @param lut LUT structure to load into
 * @param filename Path to LUT file
 * @return 0 on success, -1 on error
 */
int clebsch_lut_load_from_file(ClebschLUT* lut, const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* NEG_CLEBSCH_H */
