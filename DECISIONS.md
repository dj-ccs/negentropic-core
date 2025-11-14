# Architectural Decisions

This document records fundamental architectural decisions for `negentropic-core` v0.1.0.

## Core Principles

### 1. Functional Purity, Pointer Stability, and Determinism

**Decision**: Negentropic-core is functionally pure, pointer-stable, and deterministic across all platforms.

**Rationale**:
- **Functionally pure**: All solvers and integrators are pure functions with no side effects. This ensures reproducibility and enables easy testing.
- **Pointer-stable**: Once allocated, the simulation's memory layout never changes. All pointers into the state remain valid for the lifetime of the simulation.
- **Deterministic**: Given the same initial state and inputs, the simulation produces identical results across all platforms (x86, ARM, WASM, ESP32).

**Implications**:
- No `stdlib.h` `rand()` - use `NegRNG` from `src/core/rng.c` exclusively
- No hidden memory allocations after initialization
- Binary serialization format is platform-independent
- All intermediate math uses `double` precision, downcast only at final step

### 2. Timestamp Unit

**Decision**: The timestamp unit for `NegStateHeader.timestamp` will be **milliseconds since the Unix epoch**.

**Rationale**:
- Millisecond precision is sufficient for most simulation timesteps (typically 1-100ms)
- 64-bit milliseconds allows for ~584 million years of simulation time
- Easy conversion to/from human-readable formats
- Compatible with JavaScript `Date.now()` for WASM integration

**Implications**:
- Internal simulation time (`SimulationInternal.timestamp`) uses microseconds for precision
- Binary serialization converts microseconds → milliseconds
- Deserialization converts milliseconds → microseconds

### 3. State Versioning and Binary Format

**Decision**: All binary state serialization includes a mandatory header with MAGIC, VERSION, TIMESTAMP, HASH, and DATA_SIZE.

**Format**:
```
[8 bytes] MAGIC: "NEGSTATE"
[4 bytes] VERSION: NEG_STATE_VERSION (currently 1)
[8 bytes] TIMESTAMP: milliseconds since Unix epoch
[8 bytes] HASH: XXH3 hash of entire state
[4 bytes] DATA_SIZE: size of data section in bytes
[variable] DATA: actual state data
```

**Rationale**:
- MAGIC enables quick validation of buffer contents
- VERSION allows for schema evolution
- HASH enables detection of silent corruption
- DATA_SIZE enables safe deserialization

### 4. Error Handling Philosophy

**Decision**: Use accumulating error flags (`NegErrorFlags`) instead of exceptions or early returns.

**Rationale**:
- Compatible with C (no exceptions)
- Allows simulation to continue despite warnings
- Provides detailed diagnostics for post-mortem analysis
- Separates severity levels (warning, critical, fatal)

**Usage**:
- All numerical issues (overflow, NaN, SO(3) drift) set error flags
- Simulation continues unless fatal error occurs
- Error flags accumulate over time
- Caller can query via `neg_get_error_flags()`

### 5. Random Number Generation

**Decision**: Use deterministic `xorshift64*` RNG exclusively. No `stdlib.h rand()`.

**Rationale**:
- Deterministic: Same seed → same sequence across all platforms
- Fast: Single 64-bit operation per sample
- Well-tested: Algorithm from Vigna (2016)
- Portable: Pure integer arithmetic, no platform-specific dependencies

**Implementation**:
- Each simulation has its own `NegRNG` instance
- Default seed: `0xDEADBEEFCAFEBABE`
- Zero seed automatically replaced with default seed

### 6. Precision and Math Patterns

**Decision**: All intermediate calculations use `double` precision. Results are downcast to fixed-point or `float` only as the final step.

**Rationale**:
- Minimizes accumulated error in long simulations
- Fixed-point (16.16) has limited range and precision
- Double-precision provides 15-17 decimal digits
- Final downcast is explicit and controlled

**Pattern**:
```c
// CORRECT
double x = compute_intermediate_value_fp64(...);
double y = another_calculation_fp64(x);
fixed_t result = FLOAT_TO_FIXED(y);  // Downcast only at end

// INCORRECT
fixed_t x = compute_intermediate_value(...);  // Loses precision
fixed_t y = another_calculation(x);
```

### 7. Platform Compatibility

**Decision**: Support 4 primary targets: Unity (Windows/macOS/Linux), WASM, ESP32, and headless CI.

**Build Flags**:
- `NEGENTROPIC_CORE_ENABLED`: Master kill switch (allows Unity fallback)
- `BUILD_WASM`: Enable WebAssembly target
- `BUILD_TESTS`: Build test executables
- `BUILD_SHARED_LIBS`: Build shared library (in addition to static)

**Alignment**: Use `NEG_ALIGN16`, `NEG_ALIGN32`, `NEG_ALIGN64` macros from `platform.h` for cross-platform compatibility.

### 8. Memory Management

**Decision**: Single contiguous memory allocation per simulation. No dynamic allocation during simulation.

**Layout**:
```
[SimulationInternal struct]
[se3_pose_t array]
[float scalar_fields array]
```

**Rationale**:
- Cache-friendly (all data in one block)
- Pointer-stable (no reallocations)
- Easy to serialize (single `memcpy`)
- Predictable memory usage

### 9. JSON Output Format

**Decision**: Encode 64-bit hashes as hex strings (e.g., `"0x0123456789abcdef"`) in JSON output.

**Rationale**:
- JavaScript's `Number` type only has 53-bit precision
- Hex strings preserve full 64-bit value
- Compatible with all JSON parsers
- Human-readable for debugging

**Example**:
```json
{
  "timestamp": 1234567890,
  "version": 1,
  "hash": "0x0123456789abcdef"
}
```

---

## Version History

- **v0.1.0** (2025-11-14): Initial architectural decisions

---

**Last Updated**: 2025-11-14
**Status**: Active
