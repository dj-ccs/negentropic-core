/*
 * neg_error.h - Error Handling and Reporting
 *
 * Defines error flags and reporting mechanisms for numerical issues
 * such as overflow, underflow, NaN, and integration failures.
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#ifndef NEG_ERROR_H
#define NEG_ERROR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ERROR FLAGS
 * ======================================================================== */

/**
 * Numerical error flags.
 *
 * These flags accumulate during simulation and can be queried to detect
 * numerical issues. Multiple flags can be set simultaneously.
 */
typedef struct {
    /* Numerical errors */
    uint32_t overflow : 1;           /* Fixed-point or floating-point overflow */
    uint32_t underflow : 1;          /* Fixed-point or floating-point underflow */
    uint32_t nan_detected : 1;       /* NaN value detected */
    uint32_t inf_detected : 1;       /* Infinity detected */

    /* Integration errors */
    uint32_t so3_drift : 1;          /* SO(3) matrix drift from orthogonality */
    uint32_t energy_drift : 1;       /* Excessive energy drift */
    uint32_t step_failed : 1;        /* Integration step failed */

    /* Solver errors */
    uint32_t mass_violation : 1;     /* Mass conservation violated */
    uint32_t convergence_failed : 1; /* Iterative solver failed to converge */

    /* System errors */
    uint32_t memory_error : 1;       /* Memory allocation failed */
    uint32_t invalid_state : 1;      /* Invalid state detected */

    /* Reserved for future use */
    uint32_t _reserved : 21;

    /* Error counters */
    uint32_t total_errors;           /* Total number of errors accumulated */
    uint32_t last_error_step;        /* Simulation step where last error occurred */
} NegErrorFlags;

/* ========================================================================
 * ERROR SEVERITY LEVELS
 * ======================================================================== */

typedef enum {
    NEG_ERROR_NONE = 0,      /* No error */
    NEG_ERROR_WARNING = 1,   /* Warning: Recoverable error */
    NEG_ERROR_CRITICAL = 2,  /* Critical: May affect accuracy */
    NEG_ERROR_FATAL = 3      /* Fatal: Simulation must stop */
} NegErrorSeverity;

/* ========================================================================
 * ERROR MANAGEMENT FUNCTIONS
 * ======================================================================== */

/**
 * Initialize error flags to zero.
 */
void neg_error_init(NegErrorFlags* flags);

/**
 * Check if any error flags are set.
 *
 * @return true if any error flag is set, false otherwise
 */
bool neg_error_has_errors(const NegErrorFlags* flags);

/**
 * Get the highest severity level of current errors.
 */
NegErrorSeverity neg_error_get_severity(const NegErrorFlags* flags);

/**
 * Clear all error flags.
 */
void neg_error_clear(NegErrorFlags* flags);

/**
 * Get human-readable error message.
 *
 * @param flags Error flags
 * @param buffer Output buffer (caller-allocated)
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 */
int neg_error_to_string(const NegErrorFlags* flags, char* buffer, size_t buffer_size);

/* ========================================================================
 * ERROR SETTING MACROS (for internal use)
 * ======================================================================== */

#define NEG_SET_ERROR(flags, error_name) do { \
    (flags)->error_name = 1; \
    (flags)->total_errors++; \
} while(0)

#define NEG_CLEAR_ERROR(flags, error_name) do { \
    (flags)->error_name = 0; \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* NEG_ERROR_H */
