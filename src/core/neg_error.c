/*
 * neg_error.c - Error Handling Implementation
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#include "include/neg_error.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * ERROR MANAGEMENT FUNCTIONS
 * ======================================================================== */

void neg_error_init(NegErrorFlags* flags) {
    if (!flags) return;
    memset(flags, 0, sizeof(NegErrorFlags));
}

bool neg_error_has_errors(const NegErrorFlags* flags) {
    if (!flags) return false;
    return flags->total_errors > 0;
}

NegErrorSeverity neg_error_get_severity(const NegErrorFlags* flags) {
    if (!flags) return NEG_ERROR_NONE;

    /* Fatal errors */
    if (flags->memory_error || flags->invalid_state) {
        return NEG_ERROR_FATAL;
    }

    /* Critical errors */
    if (flags->nan_detected || flags->inf_detected ||
        flags->step_failed || flags->mass_violation) {
        return NEG_ERROR_CRITICAL;
    }

    /* Warnings */
    if (flags->overflow || flags->underflow ||
        flags->so3_drift || flags->energy_drift ||
        flags->convergence_failed) {
        return NEG_ERROR_WARNING;
    }

    return NEG_ERROR_NONE;
}

void neg_error_clear(NegErrorFlags* flags) {
    if (!flags) return;
    neg_error_init(flags);
}

int neg_error_to_string(const NegErrorFlags* flags, char* buffer, size_t buffer_size) {
    if (!flags || !buffer || buffer_size == 0) return 0;

    if (!neg_error_has_errors(flags)) {
        return snprintf(buffer, buffer_size, "No errors");
    }

    char* ptr = buffer;
    size_t remaining = buffer_size;
    int written = 0;

    written = snprintf(ptr, remaining, "Errors detected: ");
    ptr += written;
    remaining -= written;

    if (flags->overflow && remaining > 0) {
        written = snprintf(ptr, remaining, "OVERFLOW ");
        ptr += written;
        remaining -= written;
    }

    if (flags->underflow && remaining > 0) {
        written = snprintf(ptr, remaining, "UNDERFLOW ");
        ptr += written;
        remaining -= written;
    }

    if (flags->nan_detected && remaining > 0) {
        written = snprintf(ptr, remaining, "NAN ");
        ptr += written;
        remaining -= written;
    }

    if (flags->inf_detected && remaining > 0) {
        written = snprintf(ptr, remaining, "INF ");
        ptr += written;
        remaining -= written;
    }

    if (flags->so3_drift && remaining > 0) {
        written = snprintf(ptr, remaining, "SO3_DRIFT ");
        ptr += written;
        remaining -= written;
    }

    if (flags->energy_drift && remaining > 0) {
        written = snprintf(ptr, remaining, "ENERGY_DRIFT ");
        ptr += written;
        remaining -= written;
    }

    if (flags->step_failed && remaining > 0) {
        written = snprintf(ptr, remaining, "STEP_FAILED ");
        ptr += written;
        remaining -= written;
    }

    if (flags->mass_violation && remaining > 0) {
        written = snprintf(ptr, remaining, "MASS_VIOLATION ");
        ptr += written;
        remaining -= written;
    }

    if (flags->convergence_failed && remaining > 0) {
        written = snprintf(ptr, remaining, "CONVERGENCE_FAILED ");
        ptr += written;
        remaining -= written;
    }

    if (flags->memory_error && remaining > 0) {
        written = snprintf(ptr, remaining, "MEMORY_ERROR ");
        ptr += written;
        remaining -= written;
    }

    if (flags->invalid_state && remaining > 0) {
        written = snprintf(ptr, remaining, "INVALID_STATE ");
        ptr += written;
        remaining -= written;
    }

    return (int)(ptr - buffer);
}
