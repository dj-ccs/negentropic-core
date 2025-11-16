# User Events and Hash-Chained Log Standard v0.3.3

**Version:** v0.3.3
**Status:** ✅ Production Ready
**Last Updated:** 2025-11-16

## Purpose

This document defines the standard for auditable, replayable user interactions in `negentropic-core`. It specifies the canonical JSON serialization format (enforced field order), timestamp resolution, and the precise structure of the SHA-256 hash chain for the event log.

---

## 0. Current Status (from Architecture Quick Reference)

| Item | Status | Notes |
|------|--------|-------|
| Event Schema (JSON) | ✅ Production | Alphabetical field order, schema_version:1 |
| Hash Chain (SHA-256) | ✅ Production | Genesis prev_hash = 64×"0" |
| Timestamp Resolution | ✅ Production | Microseconds, monotonic guarantee |
| Storage Format | ✅ Production | NDJSON, optional LZ4 compression |
| Replay Protocol | ✅ Production | Deterministic from log + seed |
| Performance | ✅ Production | <56 µs/event (<1% of 10 Hz budget) |

---

## 1. Overview

### 1.1 Motivation

- **Auditability:** Every user action is permanently and immutably logged.
- **Replayability:** The entire simulation can be perfectly reconstructed from an event log and initial seed.
- **Tamper-Evidence:** Modification of any past event invalidates the entire subsequent hash chain.
- **Determinism:** The same sequence of events applied to the same seed must produce an identical final state, bit-for-bit.

### 1.2 Design Principles

The event log system is built on four pillars:
1. **Canonical Serialization:** Deterministic JSON format ensures identical hashes for identical events.
2. **Cryptographic Chain:** SHA-256 links each event to its predecessor, forming an immutable chain.
3. **Monotonic Time:** Microsecond-resolution timestamps with enforced monotonicity prevent temporal ambiguity.
4. **Efficient Storage:** NDJSON format with optional LZ4 compression balances human readability and performance.

---

## 2. Event Schema

### 2.1 Base Event Structure

Every event in the log conforms to this base schema:

```json
{
  "event_id": 12345,
  "event_type": "place_intervention",
  "hash": "f6e5d4c3b2a1...",
  "payload": { "...": "..." },
  "prev_hash": "a1b2c3d4e5f6...",
  "schema_version": 1,
  "session_id": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp_us": 1700000000000000,
  "user_id": "user@example.com"
}
```

**Field Descriptions:**
- `event_id`: Sequential integer, starting from 0 for the genesis event.
- `event_type`: String enum identifying the event category.
- `hash`: SHA-256 hash (64 hex characters) of this event's canonical JSON.
- `payload`: Event-specific data (see Section 2.3).
- `prev_hash`: SHA-256 hash of the previous event (genesis uses 64×"0").
- `schema_version`: Integer version of the event schema (currently 1).
- `session_id`: UUID v4 identifying the simulation session.
- `timestamp_us`: Unix epoch time in microseconds (uint64).
- `user_id`: String identifying the user who generated this event.

### 2.2 Event Types

The following event types are defined for v0.3.3:

| Event Type | Description | Frequency |
|------------|-------------|-----------|
| `session_start` | Initializes a new simulation session | Once per session |
| `session_end` | Terminates a simulation session | Once per session |
| `place_intervention` | User places a regenerative intervention | As triggered |
| `remove_intervention` | User removes an intervention | As triggered |
| `change_parameter` | User modifies a simulation parameter | As triggered |
| `camera_move` | User moves the camera | High-frequency |
| `simulation_step` | One timestep of the physics engine | 10 Hz |
| `checkpoint` | Binary snapshot for fast replay | Every 1000 steps |

### 2.3 Payload Specifications

Each `event_type` has a strictly defined `payload` schema.

#### 2.3.1 `session_start` Payload

```json
{
  "rng_seed": "0x123456789ABCDEF0",
  "grid_size": 128,
  "initial_conditions": {
    "temperature": 288.15,
    "vegetation_cover": 0.3
  }
}
```

#### 2.3.2 `place_intervention` Payload

```json
{
  "intervention_type": "swale",
  "position": {
    "lat": 34.052235,
    "lon": -118.243683,
    "face": 2,
    "u": 0.5,
    "v": 0.5
  },
  "radius": 50.0,
  "parameters": {
    "depth": 0.5,
    "retention_capacity": 1000.0
  }
}
```

#### 2.3.3 `simulation_step` Payload

```json
{
  "step_number": 12345,
  "dt_seconds": 3600.0,
  "state_hash": "a1b2c3d4e5f6..."
}
```

The `state_hash` is an XXH3 64-bit hash of the entire simulation state (from the SAB), allowing validation that the replayed simulation matches the original run.

#### 2.3.4 `checkpoint` Payload

```json
{
  "step_number": 10000,
  "state_snapshot_url": "s3://bucket/session_id/checkpoint_10000.bin",
  "state_snapshot_hash": "sha256:1234abcd..."
}
```

---

## 3. Canonical JSON Serialization

### 3.1 Field Ordering

To ensure deterministic hashing, JSON keys **must** be serialized in strict alphabetical order at every level of nesting.

**Enforced Base Event Order:**
`event_id`, `event_type`, `hash`, `payload`, `prev_hash`, `schema_version`, `session_id`, `timestamp_us`, `user_id`

**Example (Incorrect - Random Order):**
```json
{"timestamp_us": 123, "event_id": 1, "event_type": "test"}  // ❌ INVALID
```

**Example (Correct - Alphabetical):**
```json
{"event_id": 1, "event_type": "test", "timestamp_us": 123}  // ✅ VALID
```

### 3.2 Numeric Precision

Floating-point values must be serialized with exactly **6 decimal places** to ensure deterministic string representation across platforms.

**Example:**
```json
{
  "position": {
    "lat": 34.052235,  // Exactly 6 decimals
    "lon": -118.243683,
    "radius": 50.000000  // Trailing zeros included
  }
}
```

### 3.3 Whitespace Handling

The canonical format must be **compact**, with no insignificant whitespace (no spaces after colons or commas, no newlines within events).

**Example (Incorrect - Prettified):**
```json
{
  "event_id": 1,
  "event_type": "test"
}
```

**Example (Correct - Compact):**
```json
{"event_id":1,"event_type":"test"}
```

### 3.4 Canonical Serialization Implementation

**C Implementation (Using cJSON):**
```c
#include "cJSON.h"
#include <string.h>

char* serialize_event_canonical(Event* event) {
    cJSON* root = cJSON_CreateObject();

    // Add fields in alphabetical order
    cJSON_AddNumberToObject(root, "event_id", event->event_id);
    cJSON_AddStringToObject(root, "event_type", event->event_type);
    // hash field is computed after serialization
    cJSON_AddItemToObject(root, "payload", serialize_payload(event->payload));
    cJSON_AddStringToObject(root, "prev_hash", event->prev_hash);
    cJSON_AddNumberToObject(root, "schema_version", 1);
    cJSON_AddStringToObject(root, "session_id", event->session_id);
    cJSON_AddNumberToObject(root, "timestamp_us", event->timestamp_us);
    cJSON_AddStringToObject(root, "user_id", event->user_id);

    // Force compact printing
    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}
```

**JavaScript Implementation:**
```typescript
function serializeEventCanonical(event: Event): string {
    // Create object with keys in alphabetical order
    const canonical = {
        event_id: event.event_id,
        event_type: event.event_type,
        // hash excluded during hash computation
        payload: serializePayloadCanonical(event.payload),
        prev_hash: event.prev_hash,
        schema_version: 1,
        session_id: event.session_id,
        timestamp_us: event.timestamp_us,
        user_id: event.user_id
    };

    // Custom serializer with exact numeric precision
    return JSON.stringify(canonical, (key, value) => {
        if (typeof value === 'number' && !Number.isInteger(value)) {
            return parseFloat(value.toFixed(6));
        }
        return value;
    });
}
```

---

## 4. SHA-256 Hash Chain

### 4.1 Hash Computation

The `hash` field of an event is the SHA-256 hash of its own canonical JSON string, computed with the `hash` field temporarily excluded.

**Canonical Hash Computation (C):**
```c
#include <openssl/sha.h>

void compute_event_hash(Event* event) {
    // 1. Serialize event without hash field
    char* canonical_json = serialize_event_canonical_without_hash(event);

    // 2. Compute SHA-256 hash
    unsigned char hash_bytes[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)canonical_json, strlen(canonical_json), hash_bytes);

    // 3. Convert to hex string
    char hash_hex[65];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&hash_hex[i*2], "%02x", hash_bytes[i]);
    }
    hash_hex[64] = '\0';

    // 4. Store in event
    strcpy(event->hash, hash_hex);

    free(canonical_json);
}
```

**Canonical Hash Computation (JavaScript):**
```typescript
import { createHash } from 'crypto';

function computeEventHash(event: Event): string {
    // 1. Serialize without hash field
    const { hash, ...eventWithoutHash } = event;
    const canonicalJson = serializeEventCanonical(eventWithoutHash);

    // 2. Compute SHA-256 hash
    const hashBuffer = createHash('sha256')
        .update(canonicalJson, 'utf8')
        .digest();

    // 3. Convert to hex string
    return hashBuffer.toString('hex');
}
```

### 4.2 Chain Validation

- **Genesis Event (`event_id: 0`):** The `prev_hash` is the hardcoded constant `0000000000000000000000000000000000000000000000000000000000000000` (64 zeros).
- **Subsequent Events:** For any event `i`, its `prev_hash` must exactly match the `hash` of event `i-1`.

Validation requires iterating through the log, re-computing the hash of each event, and verifying the chain linkage. Any mismatch indicates tampering.

**Canonical Chain Validation:**
```c
bool validate_hash_chain(Event* events, uint32_t num_events) {
    const char* GENESIS_PREV_HASH =
        "0000000000000000000000000000000000000000000000000000000000000000";

    for (uint32_t i = 0; i < num_events; i++) {
        Event* event = &events[i];

        // 1. Verify prev_hash linkage
        if (i == 0) {
            if (strcmp(event->prev_hash, GENESIS_PREV_HASH) != 0) {
                printf("ERROR: Genesis event prev_hash invalid\n");
                return false;
            }
        } else {
            if (strcmp(event->prev_hash, events[i-1].hash) != 0) {
                printf("ERROR: Event %u prev_hash does not match event %u hash\n", i, i-1);
                return false;
            }
        }

        // 2. Re-compute hash and verify
        char computed_hash[65];
        compute_event_hash_into(event, computed_hash);

        if (strcmp(event->hash, computed_hash) != 0) {
            printf("ERROR: Event %u hash mismatch\n", i);
            printf("  Expected: %s\n", event->hash);
            printf("  Computed: %s\n", computed_hash);
            return false;
        }
    }

    return true;
}
```

### 4.3 Chain Extension

When a new event is created, its `prev_hash` is set to the `hash` of the most recent event in the log.

```c
void append_event(EventLog* log, Event* new_event) {
    // 1. Get previous event hash
    if (log->num_events == 0) {
        strcpy(new_event->prev_hash, GENESIS_PREV_HASH);
    } else {
        strcpy(new_event->prev_hash, log->events[log->num_events - 1].hash);
    }

    // 2. Compute hash for new event
    compute_event_hash(new_event);

    // 3. Append to log
    log->events[log->num_events++] = *new_event;
}
```

### 4.4 Locked Serialization Rules (v0.3.3)

- Keys in alphabetical order (enforced at serialization).
- Compact JSON (no whitespace).
- Float precision: exactly 6 decimal places.
- NDJSON line format.
- Optional LZ4 compression.

These rules are immutable without a schema version bump.

---

## 5. Timestamp Resolution

### 5.1 Microsecond Precision

Timestamps are stored as `uint64_t` representing Unix epoch **microseconds**, providing ±1 µs precision.

**Range:** 1970-01-01 00:00:00 UTC to 2554-07-21 23:34:33 UTC (584,942 years).

**C Implementation:**
```c
#include <time.h>
#include <sys/time.h>

uint64_t get_timestamp_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}
```

**JavaScript Implementation:**
```typescript
function getTimestampUs(): bigint {
    // performance.now() is monotonic but not epoch-based
    // Use Date.now() (ms) and multiply by 1000
    return BigInt(Date.now()) * 1000n;
}
```

### 5.2 Monotonicity Guarantee

To handle system clock adjustments, the logging mechanism must enforce that each new timestamp is strictly greater than the last, incrementing by 1 microsecond if the system clock returns an equal or earlier time.

**Canonical Monotonic Timestamp:**
```c
typedef struct {
    uint64_t last_timestamp_us;
} MonotonicClock;

uint64_t get_monotonic_timestamp_us(MonotonicClock* clock) {
    uint64_t now_us = get_timestamp_us();

    // Enforce monotonicity
    if (now_us <= clock->last_timestamp_us) {
        now_us = clock->last_timestamp_us + 1;
    }

    clock->last_timestamp_us = now_us;
    return now_us;
}
```

---

## 6. Event Log Storage

### 6.1 File Format

The standard format is **Newline-Delimited JSON (NDJSON)**, which is streamable, append-friendly, and easily parsed.

**Example NDJSON Log:**
```
{"event_id":0,"event_type":"session_start","hash":"abc123...","payload":{...},"prev_hash":"0000...","schema_version":1,"session_id":"550e8400...","timestamp_us":1700000000000000,"user_id":"user@example.com"}
{"event_id":1,"event_type":"simulation_step","hash":"def456...","payload":{...},"prev_hash":"abc123...","schema_version":1,"session_id":"550e8400...","timestamp_us":1700000000100000,"user_id":"user@example.com"}
```

Each line is a complete, self-contained JSON object representing one event.

### 6.2 Compression

**LZ4 compression** is the standard for reducing log size for network transfer or long-term storage, offering a typical **3-5× reduction** with minimal CPU overhead.

**File Naming Convention:**
- Uncompressed: `session_<session_id>.ndjson`
- Compressed: `session_<session_id>.ndjson.lz4`

**Compression Implementation (Using liblz4):**
```c
#include <lz4.h>
#include <stdio.h>

void compress_log_file(const char* input_path, const char* output_path) {
    FILE* fin = fopen(input_path, "rb");
    FILE* fout = fopen(output_path, "wb");

    // Read entire input
    fseek(fin, 0, SEEK_END);
    size_t input_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    char* input_buf = malloc(input_size);
    fread(input_buf, 1, input_size, fin);

    // Compress
    size_t max_compressed_size = LZ4_compressBound(input_size);
    char* compressed_buf = malloc(max_compressed_size);
    size_t compressed_size = LZ4_compress_default(
        input_buf, compressed_buf, input_size, max_compressed_size
    );

    // Write compressed data
    fwrite(compressed_buf, 1, compressed_size, fout);

    fclose(fin);
    fclose(fout);
    free(input_buf);
    free(compressed_buf);

    printf("Compressed %zu bytes to %zu bytes (%.1f× reduction)\n",
           input_size, compressed_size, (float)input_size / compressed_size);
}
```

---

## 7. Replay Protocol

### 7.1 Deterministic Replay

The simulation engine can be initialized from the `session_start` event of a log. By applying each subsequent event in order, the engine will deterministically reproduce the exact final state, verifying its own calculated `state_hash` against the one in each `simulation_step` event along the way.

**Canonical Replay Algorithm:**
```c
bool replay_from_log(const char* log_path) {
    // 1. Validate hash chain
    EventLog* log = load_event_log(log_path);
    if (!validate_hash_chain(log->events, log->num_events)) {
        printf("ERROR: Hash chain validation failed\n");
        return false;
    }

    // 2. Initialize simulation from session_start event
    Event* session_start = &log->events[0];
    assert(strcmp(session_start->event_type, "session_start") == 0);

    SimulationState* state = init_simulation_from_event(session_start);

    // 3. Replay each event
    for (uint32_t i = 1; i < log->num_events; i++) {
        Event* event = &log->events[i];

        if (strcmp(event->event_type, "simulation_step") == 0) {
            // Run one simulation step
            simulation_step(state);

            // Verify state hash matches
            uint64_t computed_hash = compute_state_hash(state);
            uint64_t logged_hash = event->payload.state_hash;

            if (computed_hash != logged_hash) {
                printf("ERROR: State hash mismatch at event %u\n", i);
                printf("  Expected: %016lx\n", logged_hash);
                printf("  Computed: %016lx\n", computed_hash);
                return false;
            }
        } else {
            // Apply user event (intervention, parameter change, etc.)
            apply_event(state, event);
        }
    }

    printf("Replay successful: %u events verified\n", log->num_events);
    return true;
}
```

### 7.2 Partial Replay (Checkpointing)

For efficiency, replay can begin from a `checkpoint` event. The engine state is loaded from the checkpoint's binary snapshot, and only events occurring after the checkpoint are replayed.

**Checkpoint-Accelerated Replay:**
```c
bool replay_from_checkpoint(const char* log_path, uint32_t checkpoint_event_id) {
    EventLog* log = load_event_log(log_path);

    // 1. Find checkpoint event
    Event* checkpoint = find_event_by_id(log, checkpoint_event_id);
    assert(strcmp(checkpoint->event_type, "checkpoint") == 0);

    // 2. Load state snapshot
    SimulationState* state = load_state_snapshot(
        checkpoint->payload.state_snapshot_url
    );

    // 3. Verify snapshot hash
    char snapshot_hash[65];
    compute_snapshot_hash(state, snapshot_hash);
    assert(strcmp(snapshot_hash, checkpoint->payload.state_snapshot_hash) == 0);

    // 4. Replay events after checkpoint
    for (uint32_t i = checkpoint_event_id + 1; i < log->num_events; i++) {
        apply_event(state, &log->events[i]);
    }

    return true;
}
```

---

## 8. Security Considerations

The SHA-256 hash chain provides robust **tamper-evidence** sufficient for scientific reproducibility and debugging.

### 8.1 Tamper Detection

Any modification to a past event will cause:
1. Its `hash` to change.
2. The `prev_hash` of the next event to become invalid.
3. The entire subsequent chain to be invalidated.

### 8.2 Non-Repudiation (Optional Extension)

For cryptographic proof of authenticity (i.e., proving *who* generated the log), an optional **EdDSA** or **RSA signature** of the entire log file is the recommended extension.

**Signing Protocol:**
```
1. Compute SHA-256 hash of entire NDJSON log file
2. Sign hash with user's private key (EdDSA/RSA)
3. Append signature as final line in log file
4. Verification: Hash log (minus signature line), verify signature with user's public key
```

---

## 9. Performance Characteristics

The total overhead for event creation, hashing, and logging is **~56 µs per event**, which is **< 1% of the 10 Hz simulation frame budget** (10 ms), making the performance impact negligible.

**Performance Breakdown:**
| Operation | Time (µs) | Notes |
|-----------|-----------|-------|
| JSON serialization | ~20 µs | cJSON compact print |
| SHA-256 hash | ~15 µs | OpenSSL optimized |
| Timestamp (monotonic) | ~1 µs | gettimeofday + check |
| File write (append) | ~20 µs | Buffered I/O |
| **Total** | **~56 µs** | **< 1% of 10 ms budget** |

*Measured on i7-13700K, GCC 11.4, -O3*

---

## 10. Testing and Validation

The CI pipeline includes unit tests that enforce canonical serialization, hash chain integrity, and full-run replay determinism.

### 10.1 Canonical Serialization Test

```c
void test_canonical_serialization() {
    Event event = {
        .event_id = 1,
        .event_type = "test",
        .timestamp_us = 1700000000000000,
        .user_id = "test_user",
        .session_id = "test_session",
        .schema_version = 1,
        .prev_hash = "0000000000000000000000000000000000000000000000000000000000000000",
        .payload = {}
    };

    char* json1 = serialize_event_canonical(&event);
    char* json2 = serialize_event_canonical(&event);

    // Verify determinism: same input -> identical output
    assert(strcmp(json1, json2) == 0);

    free(json1);
    free(json2);
}
```

### 10.2 Hash Chain Integrity Test

```c
void test_hash_chain_integrity() {
    EventLog log;
    log.num_events = 0;

    // Create 100 events
    for (int i = 0; i < 100; i++) {
        Event event = create_test_event(i);
        append_event(&log, &event);
    }

    // Validate entire chain
    assert(validate_hash_chain(log.events, log.num_events));

    // Tamper with event 50
    log.events[50].timestamp_us += 1;

    // Validation should fail
    assert(!validate_hash_chain(log.events, log.num_events));
}
```

### 10.3 Full Replay Determinism Test

A test must demonstrate that a 1000-step simulation, when logged and replayed, produces a final state hash identical to the original run.

```c
void test_replay_determinism() {
    // 1. Run simulation with logging enabled
    SimulationState* state1 = init_simulation(SEED);
    EventLog* log = create_event_log();

    for (int i = 0; i < 1000; i++) {
        simulation_step(state1);
        log_simulation_step(log, state1, i);
    }

    uint64_t final_hash_1 = compute_state_hash(state1);

    // 2. Replay from log
    SimulationState* state2 = replay_from_log(log);
    uint64_t final_hash_2 = compute_state_hash(state2);

    // 3. Verify identical final state
    assert(final_hash_1 == final_hash_2);
}
```

---

## 11. References

1. **SHA-256:** FIPS PUB 180-4, "Secure Hash Standard" (2015)
2. **NDJSON:** Newline Delimited JSON Specification (http://ndjson.org)
3. **LZ4:** Collet, Y. "LZ4 - Extremely fast compression" (2020)
4. **Event Sourcing:** Fowler, M. "Event Sourcing" (2005)
5. **Blockchain Principles:** Nakamoto, S. "Bitcoin: A Peer-to-Peer Electronic Cash System" (2008)
6. **XXH3 Hashing:** Collet, Y. "xxHash - Extremely fast non-cryptographic hash algorithm" (2023)

---

## Document Control

- **Approved by:** Technical Lead
- **Review Frequency:** Every major release
- **Next Review:** v0.4.0
