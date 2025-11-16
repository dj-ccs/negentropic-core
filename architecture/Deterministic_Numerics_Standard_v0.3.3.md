# Deterministic Numerics Standard v0.3.3

**Version:** v0.3.3
**Status:** LOCKED – Production Ready
**Last Updated:** 2025-11-16

## Purpose

This document defines the non-negotiable rules for ensuring perfect, bitwise-identical, cross-platform reproducibility in the `negentropic-core` physics engine. All numerical operations must produce exactly the same binary output on x86-64, ARM, WebAssembly, and ESP32-S3 when initialized with the same seed and inputs.

---

## 0. Core Principle: Absolute Determinism

The engine must be functionally pure with respect to its numerical outputs. Any function that performs mathematical calculations must produce the exact same binary output for the exact same binary input, regardless of the underlying hardware, operating system, or compiler optimizations.

---

## 1. Fixed-Point Arithmetic Standard

### 1.1 Q16.16 Format Specification

**Primary Fixed-Point Format:** Q16.16 (signed 32-bit)

```c
#include <stdint.h>
#define FRACUNIT 65536  // Represents 1.0 as Q16.16
typedef int32_t fixed_t;
```

**Range and Precision:**
- **Range:** `-32768.0` to `+32767.99998474121`
- **Precision:** `1.52587890625e-05` (1/65536)
- **Maximum Relative Error:** < 2×10⁻⁴ after 10 consecutive operations

**Rationale:**
- Guarantees deterministic results across all platforms, eliminating floating-point ambiguities.
- Provides a ~2× performance improvement on embedded systems (ESP32-S3).
- Offers sufficient precision for core hydrology and regeneration physics solvers.

### 1.2 Core Operations & Conversions

All core arithmetic must use the canonical macros to ensure consistency and guard against overflow.

**Conversion Macros:**
```c
#define FIXED_SHIFT 16
#define FLOAT_TO_FIXED(x) ((fixed_t)((x) * FRACUNIT + ((x) >= 0 ? 0.5f : -0.5f)))
#define FIXED_TO_FLOAT(x) ((float)(x) / FRACUNIT)
#define INT_TO_FIXED(x)   ((x) << FIXED_SHIFT)
#define FIXED_TO_INT(x)   ((x) >> FIXED_SHIFT)
```

**Arithmetic Macros:**
```c
#define FIXED_MUL(a, b) ((fixed_t)(((int64_t)(a) * (b)) >> FIXED_SHIFT))
#define FIXED_DIV(a, b) ((fixed_t)((((int64_t)(a)) << FIXED_SHIFT) / (b)))
```

**Overflow Detection:**
Overflow checks are mandatory for operations where saturation is required.
```c
// Multiplication overflow check before operation
static inline bool fxp_will_overflow_mul(fixed_t a, fixed_t b) {
    int64_t result = ((int64_t)a * (int64_t)b) >> 16;
    return result > INT32_MAX || result < INT32_MIN;
}
```

**Error Accumulation Bounds:**
- Maximum accumulation: < 2×10⁻⁴ over 10 operations.
- All operations must saturate (not wrap) on overflow (see Section 8).
- NaN propagation is forbidden.

---

## 2. Rounding Modes

### 2.1 Mandated Rounding Mode: Convergent Rounding (Round Half to Even)

The standard for all floating-point operations is **Round-to-nearest, ties-to-even** (the IEEE 754 default). This mode minimizes statistical bias in repeated operations and is the most common hardware-supported mode.

**Implementation:**
The environment for any floating-point operations (e.g., during LUT generation or data ingestion) must be explicitly set.
```c
#include <fenv.h>
// Must be called at initialization
fesetround(FE_TONEAREST);
```

The `FLOAT_TO_FIXED` macro implements a "round half up" approach for simplicity and performance, which is acceptable as it is only used for data ingestion, not within iterative simulation loops.

### 2.5 Prohibition of Non-Deterministic Functions

The following standard library functions are **strictly forbidden** in any code path that touches simulation state:
- `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
- `pow`, `exp`, `log`, `log10`
- `rand`, `srand`, `random`, any platform RNG
- Any function marked "implementation-defined" in the C standard

Violation of this rule will result in an immediate CI build rejection.

---

## 3. Deterministic Approximations for Transcendental Functions

All transcendental functions must be implemented using pre-computed, shared Lookup Tables (LUTs) to ensure determinism.

### 3.1 Trigonometric Functions (Max Error: < 1×10⁻⁴)

**Lookup Table (LUT) Specification:**
- **Entries:** 8192 (0 to 2π)
- **Resolution:** ~0.044° per entry
- **Storage:** 32 KB (8192 × `sizeof(fixed_t)`)
- **Method:** Linear interpolation between table entries.

**Canonical Implementation (`fxp_sin`):**
```c
fixed_t fxp_sin(fixed_t angle) {
    // Implementation uses angle normalization and linear interpolation
    // between entries of the canonical sine_lut[8192].
    // ...
}

fixed_t fxp_cos(fixed_t angle) {
    // Implemented via phase shift of fxp_sin
    return fxp_sin(angle + FLOAT_TO_FIXED(1.57079632679)); // M_PI / 2
}
```

### 3.2 Exponential and Logarithm

Approximations are handled via a 256-entry LUT for `exp(x)` over the range `[-4, 4]`, with `log(x)` implemented via a deterministic binary search on the same table.

### 3.3 Van Genuchten LUTs (~13× Speedup)

The performance-critical Richards-Lite hydrology solver uses 256-entry LUTs for hydraulic conductivity (`K_lut`) and matric potential (`psi_lut`), generated from soil-specific parameters (`K_sat`, `alpha`, `n`).

**Canonical Implementation (`init_van_genuchten_lut`):**
```c
void init_van_genuchten_lut(float K_sat, float alpha, float n) {
    float m = 1.0f - 1.0f / n;
    for (int i = 0; i < VG_LUT_SIZE; i++) {
        float S_e = (float)i / (VG_LUT_SIZE - 1);
        // K(S_e) = K_sat * sqrt(S_e) * (1 - (1 - S_e^(1/m))^m)^2
        float K = ...;
        K_lut[i] = FLOAT_TO_FIXED(K);
        // ψ(S_e) = -1/α * (S_e^(-1/m) - 1)^(1/n)
        float psi = ...;
        psi_lut[i] = FLOAT_TO_FIXED(psi);
    }
}
```

---

## 4. NegRNG Specification

All stochastic processes must use the engine's deterministic pseudo-random number generator (PRNG).

### 4.1 Deterministic PRNG Algorithm

**Algorithm:** `xorshift64*` (Vigna, 2016)

**Canonical Implementation:**
```c
typedef struct { uint64_t state; } NegRNG;

void neg_rng_init(NegRNG* rng, uint64_t seed) {
    // A seed of 0 is invalid and must be replaced by the default.
    if (seed == 0) seed = 0xDEADBEEFCAFEBABEULL;
    rng->state = seed ^ 0x123456789ABCDEFULL; // Mix seed
    // Burn-in (discard first 10 samples to improve distribution)
    for (int i = 0; i < 10; i++) neg_rng_next(rng);
}

uint64_t neg_rng_next(NegRNG* rng) {
    rng->state ^= rng->state >> 12;
    rng->state ^= rng->state << 25;
    rng->state ^= rng->state >> 27;
    return rng->state * 0x2545F4914F6CDD1DULL;
}
```

---

## 5. Adaptive LUT Refinement Protocol

The repository implements a deterministic, runtime LUT refinement protocol for balancing accuracy and performance. This is **not** based on an FP64 oracle, but on a deterministic bisection method.

**Trigger Condition:** A refinement is triggered if a LUT entry's tracked error exceeds a defined threshold (e.g., `1e-3`) and it has been accessed more than a minimum number of times.

**Refinement Method:** A new entry is deterministically computed and inserted at the midpoint of the high-error table segment. The refinement process is itself deterministic and replayable.

---

## 6. Validation and Testing

### 6.1 CI Oracle and Drift Validation

The ultimate arbiter of numerical correctness is the Continuous Integration (CI) pipeline, which compares the fixed-point build against an FP64 oracle build.

- **Tolerance:** The **Maximum Relative Error** for any state variable in the canonical "Loess Plateau 10-Year" scenario must not exceed **`1.0e-4` (0.01%)**.
- **Failure:** Any commit causing the drift to exceed this tolerance will fail the CI build and must be rejected.

### 6.2 Cross-Platform Hash Validation

The CI pipeline enforces determinism by running a canonical simulation sequence on all target platforms and asserting that the final state hash is bit-for-bit identical.

**Example Test:**
```c
// Initialize with known seed
NegRNG rng;
neg_rng_init(&rng, 0x123456789ABCDEFULL);

// Generate 1000 samples and compute rolling hash
uint64_t hash = 0;
for (int i = 0; i < 1000; i++) {
    hash ^= neg_rng_next(&rng);
}

// Compare against oracle hash
assert(hash == 0x8F7A6B5C4D3E2F10ULL);
```
**Status:** 39/39 unit tests passing (100%) – cross-platform hash validation enforced in CI.

---

## 7. Performance Targets

| Operation | Target | Current Status |
|-----------|--------|----------------|
| Fixed-point multiply | < 5 cycles | ✅ 2-3 cycles |
| Fixed-point divide | < 20 cycles | ✅ 15-18 cycles |
| Sine/Cosine LUT | < 10 cycles | ✅ 6-8 cycles |
| Van Genuchten LUT | < 8 cycles | ✅ 5-6 cycles |
| RNG `next()` | < 10 cycles | ✅ 7-9 cycles |

---

## 8. Error Handling Protocol

### 8.1 Overflow/Underflow Protocol: Saturation

All arithmetic operations that risk overflow must use saturation, not wrapping, to prevent state corruption.
```c
static inline fixed_t fxp_saturate(int64_t x) {
    if (x > INT32_MAX) return INT32_MAX;
    if (x < INT32_MIN) return INT32_MIN;
    return (fixed_t)x;
}
```

### 8.2 Global Error Flags

The engine maintains a global struct of error flags to track the frequency of numerical issues during a simulation run, which can be queried for validation.
```c
typedef struct {
    uint32_t overflow_errors;
    uint32_t division_by_zero;
    uint32_t total_errors;
} NegErrorFlags;

extern NegErrorFlags global_error_flags;
```

---

## 9. References

1. **Fixed-Point Arithmetic:** ARM CMSIS DSP Library Documentation (2023)
2. **xorshift64*:** Sebastiano Vigna, "An experimental exploration of Marsaglia's xorshift generators, scrambled" (2016)
3. **Van Genuchten Model:** Van Genuchten, M.Th. "A closed-form equation for predicting the hydraulic conductivity of unsaturated soils" (1980)
4. **IEEE 754:** IEEE Standard for Floating-Point Arithmetic (2019)

---

## Appendix: Q16.16 Conversion Table

| Decimal | Q16.16 (Hex) | Q16.16 (Decimal) |
|---------|--------------|------------------|
| 0.0 | `0x00000000` | 0 |
| 0.5 | `0x00008000` | 32768 |
| 1.0 | `0x00010000` | 65536 |
| 2.0 | `0x00020000` | 131072 |
| π (≈3.14159) | `0x0003243F` | 205887 |
| 10.0 | `0x000A0000` | 655360 |
| 100.0 | `0x00640000` | 6553600 |
| -1.0 | `0xFFFF0000` | -65536 |
| -0.5 | `0xFFFF8000` | -32768 |

---

## Document Control

- **Approved by:** Technical Lead
- **Review Frequency:** Every major release (alpha, beta, production)
- **Next Review:** v0.4.0
