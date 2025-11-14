// rng.c
#include "rng.h"
#include <stdint.h>
#include <string.h>

/* xorshift64*:
 * state must be non-zero.
 * algorithm from Vigna 2014.
 *
 * Result has good statistical properties for simple simulation needs and
 * is deterministic across platforms if using uint64_t semantics.
 */

static inline uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

void rng_init(NegRNG* r, uint64_t seed) {
    if (!r) return;
    /* Avoid zero state: choose a golden-ratio derived constant if seed==0 */
    if (seed == 0ULL) {
        seed = 0x9E3779B97F4A7C15ULL; /* 64-bit golden ratio */
    }
    /* Simple splitmix64 to diffuse seed into state */
    uint64_t z = seed + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    r->s = z ? z : 0x0123456789abcdefULL;
}

uint64_t rng_next_u64(NegRNG* r) {
    uint64_t x = r->s;
    /* xorshift64* variant */
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return x * 2685821657736338717ULL;
}

uint32_t rng_next_u32(NegRNG* r) {
    return (uint32_t)(rng_next_u64(r) >> 32);
}

double rng_next_double(NegRNG* r) {
    /* Produce a double in [0,1) with 53 bits of precision:
     * take upper 53 bits of rng_next_u64 and scale.
     */
    uint64_t v = rng_next_u64(r);
    /* use top 53 bits */
    uint64_t top53 = v >> (64 - 53);
    return (double)top53 / (double)(1ULL << 53);
}

void rng_fill_bytes(NegRNG* r, void* dst, size_t n) {
    uint8_t* out = (uint8_t*)dst;
    size_t i = 0;
    while (i + 8 <= n) {
        uint64_t v = rng_next_u64(r);
        memcpy(out + i, &v, 8);
        i += 8;
    }
    if (i < n) {
        uint64_t v = rng_next_u64(r);
        memcpy(out + i, &v, n - i);
    }
}
