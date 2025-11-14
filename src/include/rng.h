// rng.h
#ifndef NEG_RNG_H
#define NEG_RNG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque RNG state (64-bit). Host may store one per simulation instance. */
typedef struct {
    uint64_t s;
} NegRNG;

/* Initialize RNG state with seed. If seed == 0, a fixed non-zero default is used. */
void rng_init(NegRNG* r, uint64_t seed);

/* Return next 64-bit pseudorandom number */
uint64_t rng_next_u64(NegRNG* r);

/* Return next 32-bit pseudorandom number */
uint32_t rng_next_u32(NegRNG* r);

/* Return double in [0,1) with 53 bits of precision */
double rng_next_double(NegRNG* r);

/* Fill buffer with pseudorandom bytes */
void rng_fill_bytes(NegRNG* r, void* dst, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* NEG_RNG_H */
