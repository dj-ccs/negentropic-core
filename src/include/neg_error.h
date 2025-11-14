// neg_error.h
#ifndef NEG_ERROR_H
#define NEG_ERROR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Non-fatal diagnostic flags recorded by the core during simulation steps.
 * The runtime or caller may query these to decide precision escalation, logging,
 * or to raise deterministic exceptions to the host.
 */
typedef struct {
    uint32_t overflow;           /* counts of fixed-point overflow events */
    uint32_t underflow;          /* counts of underflow events (if tracked) */
    uint32_t domain_error;       /* e.g. acos/asin domain violations */
    uint32_t precision_escalated;/* times engine auto-escalated from fixed->double */
} NegErrorFlags;

/* API prototypes - integrate with your SimulationState implementation.
 * These are intentionally simple: the simulation root should contain an
 * instance of NegErrorFlags that these functions manipulate.
 */

/* Clear error counters for the current simulation instance */
void neg_error_clear(NegErrorFlags* flags);

/* Increment counters (stubs) */
void neg_error_inc_overflow(NegErrorFlags* flags);
void neg_error_inc_underflow(NegErrorFlags* flags);
void neg_error_inc_domain(NegErrorFlags* flags);
void neg_error_inc_escalation(NegErrorFlags* flags);

#ifdef __cplusplus
}
#endif

#endif /* NEG_ERROR_H */
