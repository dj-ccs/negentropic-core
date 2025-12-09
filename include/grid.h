/**
 * grid.h - Genesis v3.0 Sparse Memory Model Grid Abstraction
 *
 * Implements the Genesis v3.0 architectural principle #3:
 *   "Sparse is the Default Memory Model"
 *
 * For grids larger than 256x256 (65,536 cells), the system automatically
 * switches to sparse octree representation to meet the target of <300 MB
 * for 100 kha at 1 m resolution (100,000,000 cells).
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

#ifndef INCLUDE_GRID_H
#define INCLUDE_GRID_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * GRID TYPE ENUMERATION
 * ======================================================================== */

/**
 * GridType - Memory model selector for simulation grids.
 *
 * GRID_UNIFORM:       Dense array storage (default for small grids)
 *                     Memory: O(N) where N = width * height
 *                     Best for: grids <= 256x256 (65K cells)
 *
 * GRID_SPARSE_OCTREE: Sparse octree storage (auto-selected for large grids)
 *                     Memory: O(A) where A = active cells << N
 *                     Best for: grids > 256x256, especially with sparse activity
 *                     Target: <300 MB for 100 kha at 1 m resolution
 */
typedef enum {
    GRID_UNIFORM = 0,
    GRID_SPARSE_OCTREE = 1
} GridType;

/* ========================================================================
 * GRID THRESHOLD
 * ======================================================================== */

/**
 * Auto-switch threshold for sparse octree.
 * Grids with total cells exceeding this threshold will use GRID_SPARSE_OCTREE.
 * 256 * 256 = 65,536 cells (256 KB for minimal float state)
 */
#define GRID_SPARSE_THRESHOLD (256UL * 256UL)

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

/* Forward declare Cell from hydrology_richards_lite.h to avoid circular deps */
struct Cell;
typedef struct Cell GridCell_SAB;

/* Forward declare octree node from sparse_octree.h */
struct OctreeNode;

/* ========================================================================
 * GRID STRUCTURE
 * ======================================================================== */

/**
 * Grid - Abstract grid container supporting both dense and sparse storage.
 *
 * The grid automatically selects the appropriate storage type based on
 * dimensions. For uniform grids, cells are stored in a flat array.
 * For sparse grids, an octree manages active cell allocation.
 */
typedef struct Grid {
    /* Grid type (auto-selected based on dimensions) */
    GridType type;

    /* Grid dimensions */
    int nx;         /* Width in cells */
    int ny;         /* Height in cells */
    int nz;         /* Depth in layers (vertical) */

    /* Cell spacing (meters) */
    float dx;       /* Horizontal spacing */
    float dy;       /* Horizontal spacing (may differ for non-square) */
    float dz;       /* Vertical layer thickness */

    /* Storage pointers (mutually exclusive based on type) */
    GridCell_SAB* cells;        /* Dense array (GRID_UNIFORM only) */
    struct OctreeNode* octree_root;  /* Sparse octree root (GRID_SPARSE_OCTREE only) */

    /* Sparse grid metadata */
    uint32_t active_count;      /* Number of currently active cells */
    size_t memory_budget;       /* Maximum memory allocation (bytes) */

    /* Version tracking for deterministic replay */
    uint32_t version;           /* Incremented on structural changes */

} Grid;

/* ========================================================================
 * GRID LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * Create a new grid with automatic type selection.
 *
 * Allocates a Grid structure and underlying storage. For grids where
 * nx * ny > GRID_SPARSE_THRESHOLD, automatically uses sparse octree storage.
 *
 * @param nx Width in cells
 * @param ny Height in cells
 * @return Pointer to new Grid, or NULL on allocation failure
 */
Grid* grid_create(int nx, int ny);

/**
 * Create a new grid with explicit type and vertical layers.
 *
 * @param nx Width in cells
 * @param ny Height in cells
 * @param nz Number of vertical layers
 * @param type Grid type (GRID_UNIFORM or GRID_SPARSE_OCTREE)
 * @return Pointer to new Grid, or NULL on allocation failure
 */
Grid* grid_create_ex(int nx, int ny, int nz, GridType type);

/**
 * Destroy a grid and free all associated memory.
 *
 * @param grid Grid to destroy (may be NULL)
 */
void grid_destroy(Grid* grid);

/* ========================================================================
 * GRID ACCESS FUNCTIONS
 * ======================================================================== */

/**
 * Get a cell by 2D index (surface layer).
 *
 * For uniform grids: O(1) direct array access
 * For sparse grids: O(log N) octree lookup
 *
 * @param grid Grid to access
 * @param i X index (0 to nx-1)
 * @param j Y index (0 to ny-1)
 * @return Pointer to cell, or NULL if out of bounds or not allocated
 */
GridCell_SAB* grid_get_cell(Grid* grid, int i, int j);

/**
 * Get a cell by 3D index (with vertical layer).
 *
 * @param grid Grid to access
 * @param i X index (0 to nx-1)
 * @param j Y index (0 to ny-1)
 * @param k Z index (0 to nz-1)
 * @return Pointer to cell, or NULL if out of bounds or not allocated
 */
GridCell_SAB* grid_get_cell_3d(Grid* grid, int i, int j, int k);

/**
 * Activate a cell in a sparse grid (no-op for uniform grids).
 *
 * For sparse grids, allocates storage for a previously inactive cell.
 * The cell is initialized with default values.
 *
 * @param grid Grid to modify
 * @param i X index
 * @param j Y index
 * @return Pointer to activated cell, or NULL on failure
 */
GridCell_SAB* grid_activate_cell(Grid* grid, int i, int j);

/**
 * Deactivate a cell in a sparse grid (no-op for uniform grids).
 *
 * Releases storage for a cell that is no longer needed.
 *
 * @param grid Grid to modify
 * @param i X index
 * @param j Y index
 */
void grid_deactivate_cell(Grid* grid, int i, int j);

/* ========================================================================
 * GRID ITERATION
 * ======================================================================== */

/**
 * Callback function type for grid iteration.
 *
 * @param cell Pointer to current cell
 * @param i X index
 * @param j Y index
 * @param user_data User-provided context
 */
typedef void (*GridCellCallback)(GridCell_SAB* cell, int i, int j, void* user_data);

/**
 * Iterate over all active cells in the grid.
 *
 * For uniform grids: iterates all cells
 * For sparse grids: iterates only allocated (active) cells
 *
 * @param grid Grid to iterate
 * @param callback Function to call for each cell
 * @param user_data Context passed to callback
 */
void grid_foreach_active(Grid* grid, GridCellCallback callback, void* user_data);

/* ========================================================================
 * GRID UTILITIES
 * ======================================================================== */

/**
 * Get the total number of cells (nx * ny * nz).
 */
static inline size_t grid_total_cells(const Grid* grid) {
    return (size_t)grid->nx * (size_t)grid->ny * (size_t)grid->nz;
}

/**
 * Get estimated memory usage in bytes.
 */
size_t grid_memory_usage(const Grid* grid);

/**
 * Check if a grid uses sparse storage.
 */
static inline int grid_is_sparse(const Grid* grid) {
    return grid->type == GRID_SPARSE_OCTREE;
}

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_GRID_H */
