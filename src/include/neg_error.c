// neg_error.c
#include "neg_error.h"
#include <string.h>

void neg_error_clear(NegErrorFlags* flags) {
    if (!flags) return;
    memset(flags, 0, sizeof(NegErrorFlags));
}

void neg_error_inc_overflow(NegErrorFlags* flags) {
    if (!flags) return;
    flags->overflow++;
}
void neg_error_inc_underflow(NegErrorFlags* flags) {
    if (!flags) return;
    flags->underflow++;
}
void neg_error_inc_domain(NegErrorFlags* flags) {
    if (!flags) return;
    flags->domain_error++;
}
void neg_error_inc_escalation(NegErrorFlags* flags) {
    if (!flags) return;
    flags->precision_escalated++;
}
