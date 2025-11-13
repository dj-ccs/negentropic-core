/*
 * t_bsp_test.c - Unit Tests for T-BSP Spatial Partitioning
 *
 * Tests for:
 *   1. T-BSP initialization and cell allocation
 *   2. Lat/lon to cell ID mapping
 *   3. Pose insertion and overflow handling
 *   4. Dateline crossing (±180° longitude wraparound)
 *   5. Polar region handling (near ±90° latitude)
 *   6. Cell handoff protocol
 *   7. Adjacent cell calculation (8-connectivity)
 *
 * Compile with:
 *   gcc -o t_bsp_test t_bsp_test.c \
 *       ../embedded/se3_math.c ../embedded/trig_tables.c \
 *       ../embedded/t_bsp.c ../embedded/handoff.c \
 *       -I../embedded -lm -std=c99
 *
 * Author: ClaudeCode (based on Grok's T-BSP design)
 * Version: 1.0
 */

#include "../embedded/se3_edge.h"
#include "../embedded/t_bsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* Test statistics */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macros */
#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s\n", msg); \
    } \
} while(0)

#define TOLERANCE_FLOAT 0.01f

/* ========================================================================
 * TEST: T-BSP Initialization
 * ======================================================================== */

void test_t_bsp_init(void) {
    printf("\n[TEST] T-BSP Initialization\n");

    t_bsp_t bsp;
    fixed_t lat0 = FLOAT_TO_FIXED(47.6062f);   /* Seattle latitude */
    fixed_t lon0 = FLOAT_TO_FIXED(-122.3321f); /* Seattle longitude */

    t_bsp_init(&bsp, lat0, lon0);

    TEST_ASSERT(bsp.ref_lat == lat0, "Reference latitude set correctly");
    TEST_ASSERT(bsp.active_count == 0, "Active cell count starts at zero");

    /* Verify longitude normalization */
    fixed_t lon_test = FLOAT_TO_FIXED(200.0f);  /* Should wrap to -160° */
    t_bsp_init(&bsp, lat0, lon_test);
    float lon_normalized = FIXED_TO_FLOAT(bsp.ref_lon);
    TEST_ASSERT(fabs(lon_normalized - (-160.0f)) < TOLERANCE_FLOAT,
                "Reference longitude normalized to [-180, 180]");
}

/* ========================================================================
 * TEST: Cell ID Generation
 * ======================================================================== */

void test_cell_id_generation(void) {
    printf("\n[TEST] Cell ID Generation\n");

    t_bsp_t bsp;
    fixed_t lat0 = FLOAT_TO_FIXED(0.0f);   /* Equator */
    fixed_t lon0 = FLOAT_TO_FIXED(0.0f);   /* Prime meridian */
    t_bsp_init(&bsp, lat0, lon0);

    /* Test origin cell (0, 0) */
    uint16_t cell_origin = t_bsp_latlon_to_cell(&bsp, lat0, lon0);
    TEST_ASSERT(cell_origin == 0, "Origin maps to cell ID 0");

    /* Test cell 1 grid unit north */
    fixed_t lat_north = lat0 + FLOAT_TO_FIXED(0.09f);  /* ~10 km north */
    uint16_t cell_north = t_bsp_latlon_to_cell(&bsp, lat_north, lon0);
    TEST_ASSERT(cell_north != cell_origin, "North cell has different ID");

    /* Test cell 1 grid unit east */
    fixed_t lon_east = lon0 + FLOAT_TO_FIXED(0.09f);  /* ~10 km east at equator */
    uint16_t cell_east = t_bsp_latlon_to_cell(&bsp, lat0, lon_east);
    TEST_ASSERT(cell_east != cell_origin, "East cell has different ID");

    /* Test negative indices (south/west of origin) */
    fixed_t lat_south = lat0 - FLOAT_TO_FIXED(0.09f);
    fixed_t lon_west = lon0 - FLOAT_TO_FIXED(0.09f);
    uint16_t cell_sw = t_bsp_latlon_to_cell(&bsp, lat_south, lon_west);
    TEST_ASSERT(cell_sw != cell_origin, "Southwest cell has different ID");
}

/* ========================================================================
 * TEST: Dateline Crossing
 * ======================================================================== */

void test_dateline_crossing(void) {
    printf("\n[TEST] Dateline Crossing\n");

    t_bsp_t bsp;
    fixed_t lat0 = FLOAT_TO_FIXED(0.0f);
    fixed_t lon0 = FLOAT_TO_FIXED(179.0f);  /* Near dateline */
    t_bsp_init(&bsp, lat0, lon0);

    /* Position just west of dateline */
    fixed_t lon_west = FLOAT_TO_FIXED(179.5f);
    uint16_t cell_west = t_bsp_latlon_to_cell(&bsp, lat0, lon_west);

    /* Position just east of dateline (wraps to -179.5°) */
    fixed_t lon_east = FLOAT_TO_FIXED(-179.5f);
    uint16_t cell_east = t_bsp_latlon_to_cell(&bsp, lat0, lon_east);

    TEST_ASSERT(cell_west != cell_east, "Dateline crossing creates different cells");

    /* Test longitude normalization in handoff detection */
    fixed_t lon1 = FLOAT_TO_FIXED(179.0f);
    fixed_t lon2 = FLOAT_TO_FIXED(-179.0f);
    bool crossed = detect_dateline_cross(lon1, lon2);
    TEST_ASSERT(crossed, "Dateline crossing detected (179° → -179°)");

    /* Test no crossing for nearby longitudes */
    lon1 = FLOAT_TO_FIXED(100.0f);
    lon2 = FLOAT_TO_FIXED(110.0f);
    crossed = detect_dateline_cross(lon1, lon2);
    TEST_ASSERT(!crossed, "No dateline crossing for normal longitudes");
}

/* ========================================================================
 * TEST: Polar Regions
 * ======================================================================== */

void test_polar_regions(void) {
    printf("\n[TEST] Polar Regions\n");

    t_bsp_t bsp;
    fixed_t lat_arctic = FLOAT_TO_FIXED(85.0f);  /* Arctic Circle */
    fixed_t lon0 = FLOAT_TO_FIXED(0.0f);
    t_bsp_init(&bsp, lat_arctic, lon0);

    /* Test cell generation near North Pole */
    uint16_t cell_arctic = t_bsp_latlon_to_cell(&bsp, lat_arctic, lon0);
    TEST_ASSERT(cell_arctic != 0xFFFF, "Arctic cell ID generated");

    /* Test polar flag detection */
    uint8_t flags = compute_handoff_flags(lat_arctic, lon0, lat_arctic, lon0);
    TEST_ASSERT(flags & HANDOFF_FLAG_POLAR_REGION, "Polar region flag set for Arctic");

    /* Test non-polar region */
    fixed_t lat_mid = FLOAT_TO_FIXED(45.0f);
    flags = compute_handoff_flags(lat_mid, lon0, lat_mid, lon0);
    TEST_ASSERT(!(flags & HANDOFF_FLAG_POLAR_REGION), "Polar flag not set for mid-latitudes");
}

/* ========================================================================
 * TEST: Pose Insertion and Overflow
 * ======================================================================== */

void test_pose_insertion(void) {
    printf("\n[TEST] Pose Insertion and Overflow\n");

    t_bsp_t bsp;
    t_bsp_init(&bsp, 0, 0);

    se3_pose_t pose;
    se3_pose_identity(&pose);
    pose.mmsi = 367123456;
    pose.timestamp = time(NULL);

    /* Test first insertion */
    uint16_t cell_id = t_bsp_latlon_to_cell(&bsp, 0, 0);
    bool inserted = t_bsp_insert_pose(&bsp, cell_id, &pose);
    TEST_ASSERT(inserted, "First pose inserted successfully");
    TEST_ASSERT(bsp.active_count == 1, "Active cell count incremented");

    /* Test multiple insertions to same cell */
    for (int i = 0; i < 10; i++) {
        inserted = t_bsp_insert_pose(&bsp, cell_id, &pose);
        TEST_ASSERT(inserted, "Multiple poses inserted to same cell");
    }

    t_bsp_cell_t* cell = t_bsp_get_cell(&bsp, cell_id);
    TEST_ASSERT(cell != NULL, "Cell retrieved successfully");
    TEST_ASSERT(cell->pose_count == 11, "Pose count correct (1 + 10)");

    /* Test overflow behavior (fill to MAX_POSES_PER_CELL) */
    for (int i = 0; i < MAX_POSES_PER_CELL - 11; i++) {
        t_bsp_insert_pose(&bsp, cell_id, &pose);
    }

    cell = t_bsp_get_cell(&bsp, cell_id);
    TEST_ASSERT(cell->pose_count == MAX_POSES_PER_CELL, "Cell filled to capacity");

    /* Insert one more (should trigger overflow reset) */
    t_bsp_insert_pose(&bsp, cell_id, &pose);
    cell = t_bsp_get_cell(&bsp, cell_id);
    TEST_ASSERT(cell->pose_count == 1, "Cell reset after overflow");
}

/* ========================================================================
 * TEST: Cell Retrieval and Reset
 * ======================================================================== */

void test_cell_operations(void) {
    printf("\n[TEST] Cell Retrieval and Reset\n");

    t_bsp_t bsp;
    t_bsp_init(&bsp, 0, 0);

    se3_pose_t pose;
    se3_pose_identity(&pose);

    uint16_t cell_id = t_bsp_latlon_to_cell(&bsp, 0, 0);
    t_bsp_insert_pose(&bsp, cell_id, &pose);

    /* Test cell retrieval */
    t_bsp_cell_t* cell = t_bsp_get_cell(&bsp, cell_id);
    TEST_ASSERT(cell != NULL, "Cell retrieved successfully");
    TEST_ASSERT(cell->cell_id == cell_id, "Cell ID matches");

    /* Test non-existent cell */
    t_bsp_cell_t* null_cell = t_bsp_get_cell(&bsp, 0xBEEF);
    TEST_ASSERT(null_cell == NULL, "Non-existent cell returns NULL");

    /* Test cell reset */
    t_bsp_reset_cell(&bsp, cell_id);
    TEST_ASSERT(bsp.active_count == 0, "Active count decremented after reset");

    cell = t_bsp_get_cell(&bsp, cell_id);
    TEST_ASSERT(cell == NULL, "Reset cell no longer retrievable");
}

/* ========================================================================
 * TEST: Adjacent Cells (8-connectivity)
 * ======================================================================== */

void test_adjacent_cells(void) {
    printf("\n[TEST] Adjacent Cells (8-connectivity)\n");

    t_bsp_t bsp;
    t_bsp_init(&bsp, 0, 0);

    /* Get cell at origin */
    uint16_t cell_origin = t_bsp_latlon_to_cell(&bsp, 0, 0);

    /* Get adjacent cells */
    uint16_t neighbors[8];
    int count = 0;
    t_bsp_get_adjacent_cells(&bsp, cell_origin, neighbors, &count);

    TEST_ASSERT(count == 8, "8 neighbors found for interior cell");

    /* Verify neighbors are unique */
    for (int i = 0; i < count; i++) {
        TEST_ASSERT(neighbors[i] != cell_origin, "Neighbor not equal to center cell");
        for (int j = i + 1; j < count; j++) {
            TEST_ASSERT(neighbors[i] != neighbors[j], "Neighbors are unique");
        }
    }
}

/* ========================================================================
 * TEST: Handoff Protocol
 * ======================================================================== */

void test_handoff_protocol(void) {
    printf("\n[TEST] Handoff Protocol\n");

    se3_pose_t pose1, pose2;
    se3_pose_identity(&pose1);
    se3_pose_identity(&pose2);

    /* Test no handoff for same position */
    bool should_handoff = handoff_should_trigger(&pose1, &pose2);
    TEST_ASSERT(!should_handoff, "No handoff for identical poses");

    /* Test handoff for large distance (>10 km) */
    pose2.translation[0] = FLOAT_TO_FIXED(11000.0f);  /* 11 km east */
    should_handoff = handoff_should_trigger(&pose1, &pose2);
    TEST_ASSERT(should_handoff, "Handoff triggered for >10 km distance");

    /* Test handoff for small distance (<10 km) */
    pose2.translation[0] = FLOAT_TO_FIXED(5000.0f);  /* 5 km east */
    should_handoff = handoff_should_trigger(&pose1, &pose2);
    TEST_ASSERT(!should_handoff, "No handoff for <10 km distance");
}

/* ========================================================================
 * TEST: Handoff Packet Serialization
 * ======================================================================== */

void test_handoff_serialization(void) {
    printf("\n[TEST] Handoff Packet Serialization\n");

    handoff_packet_t pkt_orig, pkt_decoded;
    uint8_t buffer[256];

    /* Create test packet */
    se3_pose_t pose;
    se3_pose_identity(&pose);
    pose.mmsi = 367123456;
    pose.timestamp = time(NULL);

    create_handoff_packet(367123456, &pose, 0x0100, 0x0101, 0x01, &pkt_orig);

    /* Serialize */
    serialize_handoff(&pkt_orig, buffer);
    TEST_ASSERT(get_handoff_packet_size() == sizeof(handoff_packet_t),
                "Handoff packet size correct");

    /* Deserialize */
    bool success = deserialize_handoff(buffer, &pkt_decoded);
    TEST_ASSERT(success, "Handoff packet deserialized successfully");
    TEST_ASSERT(pkt_decoded.mmsi == pkt_orig.mmsi, "MMSI preserved");
    TEST_ASSERT(pkt_decoded.old_cell_id == pkt_orig.old_cell_id, "Old cell ID preserved");
    TEST_ASSERT(pkt_decoded.new_cell_id == pkt_orig.new_cell_id, "New cell ID preserved");
    TEST_ASSERT(pkt_decoded.flags == pkt_orig.flags, "Flags preserved");
}

/* ========================================================================
 * TEST: Handoff Packet Validation
 * ======================================================================== */

void test_handoff_validation(void) {
    printf("\n[TEST] Handoff Packet Validation\n");

    handoff_packet_t pkt;
    se3_pose_t pose;
    se3_pose_identity(&pose);
    pose.timestamp = time(NULL);

    create_handoff_packet(367123456, &pose, 0x0100, 0x0101, 0, &pkt);

    /* Test valid packet */
    bool valid = validate_handoff_packet(&pkt, time(NULL));
    TEST_ASSERT(valid, "Valid handoff packet accepted");

    /* Test invalid MMSI */
    pkt.mmsi = 0;
    valid = validate_handoff_packet(&pkt, time(NULL));
    TEST_ASSERT(!valid, "Zero MMSI rejected");

    /* Test same cell IDs */
    pkt.mmsi = 367123456;
    pkt.new_cell_id = pkt.old_cell_id;
    valid = validate_handoff_packet(&pkt, time(NULL));
    TEST_ASSERT(!valid, "Same cell IDs rejected");

    /* Test old timestamp */
    pkt.new_cell_id = 0x0101;
    pkt.last_pose.timestamp = time(NULL) - 100000;  /* >24 hours ago */
    valid = validate_handoff_packet(&pkt, time(NULL));
    TEST_ASSERT(!valid, "Old timestamp rejected");
}

/* ========================================================================
 * TEST: Cell Near Full Detection
 * ======================================================================== */

void test_cell_near_full(void) {
    printf("\n[TEST] Cell Near Full Detection\n");

    t_bsp_t bsp;
    t_bsp_init(&bsp, 0, 0);

    se3_pose_t pose;
    se3_pose_identity(&pose);
    uint16_t cell_id = t_bsp_latlon_to_cell(&bsp, 0, 0);

    /* Fill cell to 50% */
    for (int i = 0; i < MAX_POSES_PER_CELL / 2; i++) {
        t_bsp_insert_pose(&bsp, cell_id, &pose);
    }

    t_bsp_cell_t* cell = t_bsp_get_cell(&bsp, cell_id);

    TEST_ASSERT(!t_bsp_cell_near_full(cell, 0.9f), "Cell not near full at 50%");

    /* Fill to 95% */
    for (int i = 0; i < (MAX_POSES_PER_CELL * 95 / 100) - (MAX_POSES_PER_CELL / 2); i++) {
        t_bsp_insert_pose(&bsp, cell_id, &pose);
    }

    cell = t_bsp_get_cell(&bsp, cell_id);
    TEST_ASSERT(t_bsp_cell_near_full(cell, 0.9f), "Cell near full at 95%");
}

/* ========================================================================
 * TEST: Multiple Concurrent Cells
 * ======================================================================== */

void test_multiple_cells(void) {
    printf("\n[TEST] Multiple Concurrent Cells\n");

    t_bsp_t bsp;
    t_bsp_init(&bsp, 0, 0);

    se3_pose_t pose;
    se3_pose_identity(&pose);

    /* Create 10 different cells */
    uint16_t cell_ids[10];
    for (int i = 0; i < 10; i++) {
        fixed_t lat = FLOAT_TO_FIXED(i * 0.1f);  /* ~10 km spacing */
        cell_ids[i] = t_bsp_latlon_to_cell(&bsp, lat, 0);
        t_bsp_insert_pose(&bsp, cell_ids[i], &pose);
    }

    TEST_ASSERT(t_bsp_get_active_count(&bsp) == 10, "10 cells allocated");

    /* Verify each cell is unique */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            TEST_ASSERT(cell_ids[i] != cell_ids[j], "Cell IDs unique");
        }
    }
}

/* ========================================================================
 * TEST: Cell Bounds Computation
 * ======================================================================== */

void test_cell_bounds(void) {
    printf("\n[TEST] Cell Bounds Computation\n");

    t_bsp_t bsp;
    fixed_t lat0 = FLOAT_TO_FIXED(47.0f);
    fixed_t lon0 = FLOAT_TO_FIXED(-122.0f);
    t_bsp_init(&bsp, lat0, lon0);

    uint16_t cell_id = t_bsp_latlon_to_cell(&bsp, lat0, lon0);

    fixed_t lat_min, lat_max, lon_min, lon_max;
    t_bsp_get_cell_bounds(&bsp, cell_id, &lat_min, &lat_max, &lon_min, &lon_max);

    /* Origin should be within bounds */
    TEST_ASSERT(lat_min <= lat0 && lat0 <= lat_max, "Origin latitude within bounds");
    TEST_ASSERT(lon_min <= lon0 && lon0 <= lon_max, "Origin longitude within bounds");

    /* Bounds should be approximately CELL_SIZE_KM */
    float lat_span = FIXED_TO_FLOAT(lat_max - lat_min);
    TEST_ASSERT(fabs(lat_span - 0.09f) < 0.02f, "Cell latitude span ~0.09°");
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    srand(time(NULL));

    printf("======================================================================\n");
    printf("T-BSP SPATIAL PARTITIONING - UNIT TEST SUITE\n");
    printf("======================================================================\n");
    printf("Doom BSP → Trajectory BSP for Edge λ-Computation\n");
    printf("Target: ESP32-S3 (512KB SRAM, no malloc)\n");
    printf("Cell size: %d km, Max cells: %d, Max poses/cell: %d\n",
           CELL_SIZE_KM, MAX_CELLS, MAX_POSES_PER_CELL);

    /* Initialize SE(3) subsystem */
    se3_init_tables();

    /* Run all test suites */
    test_t_bsp_init();
    test_cell_id_generation();
    test_dateline_crossing();
    test_polar_regions();
    test_pose_insertion();
    test_cell_operations();
    test_adjacent_cells();
    test_handoff_protocol();
    test_handoff_serialization();
    test_handoff_validation();
    test_cell_near_full();
    test_multiple_cells();
    test_cell_bounds();

    /* Summary */
    printf("\n======================================================================\n");
    printf("TEST SUMMARY\n");
    printf("======================================================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("\n✓ ALL TESTS PASSED - Ready for ESP32-S3 deployment\n");
    } else {
        printf("\n✗ SOME TESTS FAILED - Review implementation\n");
    }
    printf("======================================================================\n");

    return tests_failed;
}
