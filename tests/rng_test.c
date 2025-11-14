// tests/rng_test.c
//
// Deterministic RNG reproducibility test.
// Ensures that NegRNG produces *bit-exact* sequences across platforms.
// Fails (non-zero exit code) if any deviation occurs.
//
// Compile (example):
//    gcc -I../src/include -o rng_test rng_test.c ../src/rng.c
//
// Run:
//    ./rng_test
//
// Expected output:
//    [OK] RNG determinism verified for seed=0xDEADBEEFCAFEBABE
//
// If failed:
//    [FAIL] Mismatch at index N: expected=..., got=...

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rng.h"

#define TEST_SEED 0xDEADBEEFCAFEBABEULL
#define SEQ_LEN   16

/* Reference sequence: generated ONCE on a known-good platform (Linux x86_64).
 * These constants must remain unchanged forever. If RNG changes internally,
 * bump the RNG version or provide a migration adapter.
 */
static const uint64_t REFERENCE_SEQ[SEQ_LEN] = {
    0x5fb7e3e0d2cb0c03ULL,
    0xff4bf8d3e744be9eULL,
    0xe6b09286e0e7ebdfULL,
    0xc2c724fd12c2775dULL,
    0xb0e5fc544d46777aULL,
    0x52fcb722dbb2bd03ULL,
    0x0f557f3205fbf5a3ULL,
    0x127c6d588bafcbbeULL,
    0xd52a83ca23e5ae4dULL,
    0xd61376051bd3ed2dULL,
    0x33a54a27d083fa01ULL,
    0xb098178b2a5efaa3ULL,
    0x6edb93b4dad6c7ceULL,
    0x60b643c5d59b9a1dULL,
    0xb57597e076286c63ULL,
    0xa95da412905cfad5ULL
};

int main(void) {
    NegRNG rng;
    rng_init(&rng, TEST_SEED);

    uint64_t seq[SEQ_LEN];
    for (int i = 0; i < SEQ_LEN; i++) {
        seq[i] = rng_next_u64(&rng);
    }

    for (int i = 0; i < SEQ_LEN; i++) {
        if (seq[i] != REFERENCE_SEQ[i]) {
            printf("[FAIL] Mismatch at index %d:\n", i);
            printf("       expected 0x%016llx, got 0x%016llx\n",
                   (unsigned long long)REFERENCE_SEQ[i],
                   (unsigned long long)seq[i]);
            return 1;
        }
    }

    printf("[OK] RNG determinism verified for seed=0x%016llx\n",
           (unsigned long long)TEST_SEED);
    return 0;
}
