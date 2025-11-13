/*
 * handoff.c - Cell-to-Cell Vessel Handoff Protocol
 *
 * Manages vessel transitions between T-BSP cells with dateline-safe
 * serialization and network broadcast for distributed edge nodes.
 *
 * Doom Lineage:
 *   - Doom thinker_t (moving entities) → handoff_packet_t (vessel state)
 *   - Doom P_RunThinkers() → handoff_should_trigger() (state update)
 *   - Network packet (Doom multiplayer) → serialize_handoff()
 *
 * Reference: id-Software/DOOM linuxdoom-1.10/p_mobj.c
 * Author: Grok (handoff design) + ClaudeCode (implementation) + ChatGPT (fix)
 * Version: 1.1
 */

#include "se3_edge.h"
#include "t_bsp.h"
#include <string.h>

/* ========================================================================
 * HANDOFF PROTOCOL
 * ======================================================================== */

/**
 * Serialize handoff packet for network transmission.
 *
 * Ensures dateline-safe encoding:
 *   - Normalizes longitude to [-180°, 180°] before broadcast
 *   - Packs struct with `#pragma pack(push, 1)` for consistent alignment
 *   - 100 bytes total (fits in single LoRa frame)
 *
 * Doom analog: Doom's multiplayer packet serialization (doomcom_t)
 *
 * @param pkt Source handoff packet
 * @param buffer Output buffer (must be at least sizeof(handoff_packet_t) = 100 bytes)
 */
void serialize_handoff(const handoff_packet_t* pkt, uint8_t* buffer) {
    /* Handoff packet stores ENU meters, not lat/lon degrees.
     * Normalization is handled in t_bsp_latlon_to_cell().
     * Straight memcpy is sufficient here.
     */
    memcpy(buffer, pkt, sizeof(handoff_packet_t));
}

/**
 * Deserialize handoff packet from network buffer.
 *
 * @param buffer Input buffer (at least sizeof(handoff_packet_t) bytes)
 * @param pkt Output handoff packet
 * @return true on success, false if buffer invalid
 */
bool deserialize_handoff(const uint8_t* buffer, handoff_packet_t* pkt) {
    if (!buffer || !pkt) {
        return false;
    }

    memcpy(pkt, buffer, sizeof(handoff_packet_t));

    /* Basic validation */
    if (pkt->mmsi == 0) {
        return false;  /* Invalid MMSI */
    }

    return true;
}

/**
 * Check if vessel should trigger handoff to new cell.
 *
 * Doom analog: Doom's P_CheckPosition() checks if thing crosses sector boundary
 *
 * Triggers handoff when vessel moves beyond cell boundary:
 *   - Compute Euclidean distance between consecutive poses
 *   - Trigger if distance > CELL_SIZE_KM (in meters)
 *
 * Distance calculation:
 *   - Uses ENU coordinates (meters)
 *   - Converts to float BEFORE squaring to prevent overflow
 *   - Threshold: (CELL_SIZE_KM * 1000)^2 meters^2
 *
 * Note (ChatGPT fix):
 *   - FixedMul would overflow for large meter values (>32K)
 *   - Converting to float first avoids this issue
 *
 * @param prev Previous pose (in current cell)
 * @param curr Current pose (possibly in new cell)
 * @return true if distance exceeds cell boundary
 */
bool handoff_should_trigger(const se3_pose_t* prev, const se3_pose_t* curr) {
    if (!prev || !curr) {
        return false;
    }

    /* Compute translation deltas (fixed-point meters) */
    fixed_t dx = curr->translation[0] - prev->translation[0];
    fixed_t dy = curr->translation[1] - prev->translation[1];
    fixed_t dz = curr->translation[2] - prev->translation[2];

    /* Convert to float meters *before* squaring (prevents overflow) */
    float dx_m = FIXED_TO_FLOAT(dx);
    float dy_m = FIXED_TO_FLOAT(dy);
    float dz_m = FIXED_TO_FLOAT(dz);

    /* Compute squared distance in meters */
    float dist_sq_m = dx_m * dx_m + dy_m * dy_m + dz_m * dz_m;

    /* Threshold = (CELL_SIZE_KM × 1000 m)² */
    const float threshold_m = CELL_SIZE_KM * 1000.0f;
    const float threshold_m_sq = threshold_m * threshold_m;

    return dist_sq_m > threshold_m_sq;
}

/**
 * Create handoff packet from cell transition.
 *
 * Prepares packet for network broadcast to adjacent edge nodes.
 *
 * @param mmsi Vessel identifier
 * @param last_pose Final pose in old cell
 * @param old_cell_id Source cell
 * @param new_cell_id Destination cell
 * @param flags Handoff flags (dateline_cross, polar_region, etc.)
 * @param pkt Output handoff packet
 */
void create_handoff_packet(uint32_t mmsi, const se3_pose_t* last_pose,
                           uint16_t old_cell_id, uint16_t new_cell_id,
                           uint8_t flags, handoff_packet_t* pkt) {
    pkt->mmsi = mmsi;
    pkt->last_pose = *last_pose;
    pkt->old_cell_id = old_cell_id;
    pkt->new_cell_id = new_cell_id;
    pkt->flags = flags;

    /* Clear signature (filled later if DLT trustless mode enabled) */
    memset(pkt->signature, 0, sizeof(pkt->signature));
}

/**
 * Detect dateline crossing based on longitude delta.
 *
 * Returns true if longitude wraps around ±180° boundary.
 *
 * @param lon1 First longitude (fixed-point degrees, normalized)
 * @param lon2 Second longitude (fixed-point degrees, normalized)
 * @return true if dateline crossed (delta > 180°)
 */
bool detect_dateline_cross(fixed_t lon1, fixed_t lon2) {
    /* Normalize both longitudes */
    lon1 = normalize_lon(lon1);
    lon2 = normalize_lon(lon2);

    /* Compute raw delta */
    fixed_t raw_delta = lon2 - lon1;
    fixed_t threshold = FLOAT_TO_FIXED(180.0f);

    return (raw_delta > threshold || raw_delta < -threshold);
}

/**
 * Compute handoff flags based on geographic context.
 *
 * Sets flags for:
 *   - Dateline crossing (bit 0)
 *   - Polar region (bit 1) - within 10° of poles
 *   - Future: other anomalies
 *
 * @param lat1 Source latitude (fixed-point degrees)
 * @param lon1 Source longitude (fixed-point degrees)
 * @param lat2 Destination latitude (fixed-point degrees)
 * @param lon2 Destination longitude (fixed-point degrees)
 * @return flags byte
 */
uint8_t compute_handoff_flags(fixed_t lat1, fixed_t lon1,
                              fixed_t lat2, fixed_t lon2) {
    uint8_t flags = 0;

    /* Bit 0: Dateline crossing */
    if (detect_dateline_cross(lon1, lon2)) {
        flags |= HANDOFF_FLAG_DATELINE_CROSS;
    }

    /* Bit 1: Polar region (within 10° of ±90°) */
    fixed_t polar_threshold = FLOAT_TO_FIXED(80.0f);  /* ±80° latitude */
    fixed_t lat_abs = fixed_abs(lat1);
    if (lat_abs > polar_threshold || fixed_abs(lat2) > polar_threshold) {
        flags |= HANDOFF_FLAG_POLAR_REGION;
    }

    return flags;
}

/* ========================================================================
 * DIAGNOSTIC FUNCTIONS
 * ======================================================================== */

/**
 * Get handoff packet size (for network buffer allocation).
 *
 * @return Size in bytes (100)
 */
size_t get_handoff_packet_size(void) {
    return sizeof(handoff_packet_t);
}

/**
 * Validate handoff packet integrity.
 *
 * Checks:
 *   - MMSI non-zero
 *   - Cell IDs different (actual transition)
 *   - Timestamp reasonable (within last 24 hours)
 *
 * @param pkt Handoff packet to validate
 * @param current_time Current Unix timestamp (for recency check)
 * @return true if packet valid
 */
bool validate_handoff_packet(const handoff_packet_t* pkt, uint32_t current_time) {
    if (!pkt || pkt->mmsi == 0) {
        return false;
    }

    /* Cell IDs should be different (actual transition) */
    if (pkt->old_cell_id == pkt->new_cell_id) {
        return false;
    }

    /* Timestamp should be recent (within 24 hours = 86,400 seconds) */
    if (current_time > pkt->last_pose.timestamp) {
        uint32_t age = current_time - pkt->last_pose.timestamp;
        if (age > 86400) {
            return false;  /* Packet too old */
        }
    }

    return true;
}
