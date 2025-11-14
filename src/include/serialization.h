// serialization.h
#ifndef NEG_SERIALIZATION_H
#define NEG_SERIALIZATION_H

#include <stdint.h>
#include <stddef.h>
#include "platform.h"
#include "state_versioning.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Binary header preamble for state snapshots.
 *
 * Byte layout (packed, little-endian assumed unless explicitly converted):
 * 0..7   : ASCII MAGIC "NEGSTATE" (8 bytes, no NUL)
 * 8..11  : uint32_t VERSION (NEG_STATE_VERSION)
 * 12..19 : uint64_t TIMESTAMP (unix epoch ms or ns - document convention)
 * 20..27 : uint64_t HASH (deterministic XXH3/xxhash64 of DATA)
 * 28..31 : uint32_t DATA_SIZE (number of bytes following header)
 * 32..   : DATA (binary payload)
 *
 * IMPORTANT: For cross-platform reproducibility, choose endianness (we recommend
 * little-endian). When sending across different-endian platforms, convert fields.
 */

#pragma pack(push, 1)
typedef struct {
    char     magic[8];      /* "NEGSTATE" */
    uint32_t version;       /* NEG_STATE_VERSION */
    uint64_t timestamp;     /* epoch (ms or ns) - choose convention */
    uint64_t hash;          /* deterministic state hash (xxh64 suggested) */
    uint32_t data_size;     /* bytes following this header */
    /* followed by data_size bytes of payload */
} NEG_PACKED NegStateHeader;
#pragma pack(pop)

/* Header size (should be 32 bytes in above layout) */
#define NEG_STATE_HEADER_SIZE ((size_t)32)

/* Helper prototypes (stubs) */
int neg_write_header(uint8_t* dst_buffer, size_t dst_capacity, const NegStateHeader* hdr);
int neg_read_header(const uint8_t* src_buffer, size_t src_capacity, NegStateHeader* hdr_out);

/* Convenience to initialize a header with magic/version */
static inline void neg_header_init(NegStateHeader* h, uint64_t timestamp, uint64_t hash, uint32_t data_size) {
    if (!h) return;
    h->magic[0] = 'N'; h->magic[1] = 'E'; h->magic[2] = 'G'; h->magic[3] = 'S';
    h->magic[4] = 'T'; h->magic[5] = 'A'; h->magic[6] = 'T'; h->magic[7] = 'E';
    h->version = (uint32_t)NEG_STATE_VERSION;
    h->timestamp = timestamp;
    h->hash = hash;
    h->data_size = data_size;
}

#ifdef __cplusplus
}
#endif

#endif /* NEG_SERIALIZATION_H */
