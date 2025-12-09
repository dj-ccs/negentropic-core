/**
 * sparse_octree.h - Sparse Octree Allocation Skeleton (Genesis v3.0)
 *
 * Implements the Genesis v3.0 architectural principle #3:
 *   "Sparse is the Default Memory Model"
 *
 * This header defines the octree data structures for sparse grid storage.
 * The current implementation provides only the allocation skeleton and
 * active-cell list management. Full traversal and GPU mapping are planned
 * for future sprints.
 *
 * Memory Target: <300 MB for 100 kha at 1 m resolution
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

#ifndef SRC_GRID_SPARSE_OCTREE_H
#define SRC_GRID_SPARSE_OCTREE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * OCTREE CONFIGURATION
 * ======================================================================== */

/**
 * Number of children per octree node.
 * For 2D grids (quadtree): 4
 * For 3D grids (octree): 8
 *
 * We use 8 to support future 3D extension even for 2D grids.
 */
#define OCTREE_CHILDREN 8

/**
 * Minimum leaf size (cells per leaf node).
 * Smaller values give finer granularity but more overhead.
 */
#define OCTREE_MIN_LEAF_SIZE 16

/**
 * Default memory budget for 100 kha at 1 m resolution.
 * 100 kha = 1e9 m^2 = 1e9 cells at 1m resolution
 * Target: 300 MB = 300 * 1024 * 1024 bytes
 */
#define OCTREE_DEFAULT_BUDGET (300UL * 1024UL * 1024UL)

/* ========================================================================
 * OCTREE NODE STRUCTURE
 * ======================================================================== */

/**
 * OctreeNode - Spatial subdivision node for sparse cell storage.
 *
 * Each node represents a rectangular region of the grid. Interior nodes
 * have children; leaf nodes store active cell indices.
 *
 * Memory layout optimized for cache efficiency:
 *   - Fixed-size structure (no variable-length arrays in node)
 *   - Children and indices stored as pointers for flexible allocation
 */
typedef struct OctreeNode {
    /* Bounding box (grid indices) */
    int x0, y0;     /* Lower-left corner */
    int x1, y1;     /* Upper-right corner (exclusive) */

    /* Node depth in tree (0 = root) */
    uint8_t depth;

    /* Node type flags */
    uint8_t is_leaf;        /* 1 if leaf node (stores cells), 0 if interior */
    uint8_t is_allocated;   /* 1 if this node's cells are allocated */
    uint8_t reserved;       /* Padding for alignment */

    /* Active cell tracking */
    uint32_t active_count;  /* Number of active cells in this subtree */

    /* Children (NULL if leaf) */
    struct OctreeNode* children[OCTREE_CHILDREN];

    /* Active cell indices (only for leaf nodes) */
    uint32_t* active_indices;   /* Array of linear indices for active cells */
    uint32_t active_capacity;   /* Allocated capacity of active_indices */

} OctreeNode;

/* ========================================================================
 * OCTREE ROOT STRUCTURE
 * ======================================================================== */

/**
 * Octree - Root container for sparse grid storage.
 *
 * Manages the octree hierarchy and memory allocation budget.
 */
typedef struct Octree {
    /* Root node of the tree */
    OctreeNode* root;

    /* Grid dimensions (for bounds checking) */
    int nx, ny;

    /* Memory management */
    size_t memory_budget;       /* Maximum allocation in bytes */
    size_t memory_used;         /* Current allocation in bytes */

    /* Statistics */
    uint32_t total_nodes;       /* Total allocated nodes */
    uint32_t leaf_nodes;        /* Number of leaf nodes */
    uint32_t total_active;      /* Total active cells across all leaves */

} Octree;

/* ========================================================================
 * ALLOCATION FUNCTIONS (Skeleton Implementation)
 * ======================================================================== */

/**
 * Allocate and initialize an octree root node covering the full grid.
 *
 * This is a skeleton implementation that creates the root structure
 * without building the full tree hierarchy. The tree is built lazily
 * as cells are activated.
 *
 * @param nx Grid width in cells
 * @param ny Grid height in cells
 * @param budget_bytes Maximum memory allocation (use OCTREE_DEFAULT_BUDGET)
 * @return Pointer to new OctreeNode, or NULL on failure
 */
OctreeNode* octree_alloc_root(int nx, int ny, size_t budget_bytes);

/**
 * Create a full Octree container with metadata.
 *
 * @param nx Grid width in cells
 * @param ny Grid height in cells
 * @param budget_bytes Maximum memory allocation
 * @return Pointer to new Octree, or NULL on failure
 */
Octree* octree_create(int nx, int ny, size_t budget_bytes);

/**
 * Free an octree node and all its children recursively.
 *
 * @param node Node to free (may be NULL)
 */
void octree_free_node(OctreeNode* node);

/**
 * Destroy an octree and free all associated memory.
 *
 * @param tree Octree to destroy (may be NULL)
 */
void octree_destroy(Octree* tree);

/* ========================================================================
 * ACTIVE CELL MANAGEMENT (Skeleton Implementation)
 * ======================================================================== */

/**
 * Mark a cell as active in the octree.
 *
 * This is a skeleton implementation that tracks the active cell index
 * but does not allocate cell storage (that's handled by Grid).
 *
 * @param tree Octree container
 * @param i X index
 * @param j Y index
 * @return 0 on success, -1 on failure
 */
int octree_activate_cell(Octree* tree, int i, int j);

/**
 * Mark a cell as inactive in the octree.
 *
 * @param tree Octree container
 * @param i X index
 * @param j Y index
 * @return 0 on success, -1 on failure
 */
int octree_deactivate_cell(Octree* tree, int i, int j);

/**
 * Check if a cell is active.
 *
 * @param tree Octree container
 * @param i X index
 * @param j Y index
 * @return 1 if active, 0 if inactive
 */
int octree_is_active(const Octree* tree, int i, int j);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * Get the linear index for a 2D cell coordinate.
 */
static inline uint32_t octree_linear_index(int nx, int i, int j) {
    return (uint32_t)(j * nx + i);
}

/**
 * Get 2D coordinates from linear index.
 */
static inline void octree_coords_from_index(int nx, uint32_t idx, int* i, int* j) {
    *j = (int)(idx / (uint32_t)nx);
    *i = (int)(idx % (uint32_t)nx);
}

/**
 * Get estimated memory usage for an octree.
 */
size_t octree_memory_usage(const Octree* tree);

/* ========================================================================
 * FUTURE EXTENSIONS (Not Implemented in Genesis v3.0)
 * ======================================================================== */

/*
 * The following functions are planned for future sprints:
 *
 * - octree_traverse(): Efficient tree traversal for iteration
 * - octree_query_region(): Find all active cells in a bounding box
 * - octree_balance(): Rebalance tree after many activations/deactivations
 * - octree_gpu_map(): Prepare sparse data for GPU compute
 * - octree_serialize(): Save/load octree state for checkpointing
 */

#ifdef __cplusplus
}
#endif

#endif /* SRC_GRID_SPARSE_OCTREE_H */
