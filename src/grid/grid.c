/**
 * grid.c - Genesis v3.0 Grid Implementation
 *
 * Implements the Grid abstraction with automatic type selection
 * between uniform and sparse octree storage.
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

#include "../../include/grid.h"
#include "sparse_octree.h"
#include <stdlib.h>
#include <string.h>

/* Forward declaration for Cell structure size */
/* In production this would include the actual header */
#ifndef CELL_SIZE_ESTIMATE
#define CELL_SIZE_ESTIMATE 256  /* Bytes per cell, conservative estimate */
#endif

/* ========================================================================
 * GRID CREATION
 * ======================================================================== */

Grid* grid_create(int nx, int ny) {
    /* Use single vertical layer by default */
    int nz = 1;

    /* Auto-select grid type based on size threshold */
    unsigned long total_cells = (unsigned long)nx * (unsigned long)ny;
    GridType type = (total_cells > GRID_SPARSE_THRESHOLD)
                    ? GRID_SPARSE_OCTREE
                    : GRID_UNIFORM;

    return grid_create_ex(nx, ny, nz, type);
}

Grid* grid_create_ex(int nx, int ny, int nz, GridType type) {
    /* Validate inputs */
    if (nx <= 0 || ny <= 0 || nz <= 0) {
        return NULL;
    }

    /* Allocate grid structure */
    Grid* grid = (Grid*)calloc(1, sizeof(Grid));
    if (!grid) {
        return NULL;
    }

    /* Initialize dimensions */
    grid->type = type;
    grid->nx = nx;
    grid->ny = ny;
    grid->nz = nz;

    /* Default cell spacing (1 meter) */
    grid->dx = 1.0f;
    grid->dy = 1.0f;
    grid->dz = 0.1f;  /* 10 cm vertical layers */

    /* Initialize storage */
    grid->cells = NULL;
    grid->octree_root = NULL;
    grid->active_count = 0;
    grid->version = 0;

    if (type == GRID_UNIFORM) {
        /* Allocate dense cell array */
        size_t num_cells = (size_t)nx * (size_t)ny * (size_t)nz;

        /* Note: We allocate a placeholder since Cell struct may not be available */
        /* In production, this would be: grid->cells = calloc(num_cells, sizeof(Cell)); */

        /* For skeleton, just track that we would allocate */
        grid->cells = NULL;  /* Placeholder - actual allocation in production */
        grid->memory_budget = num_cells * CELL_SIZE_ESTIMATE;
        grid->active_count = (uint32_t)num_cells;  /* All cells active in uniform grid */

    } else {  /* GRID_SPARSE_OCTREE */
        /* Create sparse octree with default memory budget */
        grid->memory_budget = OCTREE_DEFAULT_BUDGET;

        Octree* tree = octree_create(nx, ny, grid->memory_budget);
        if (!tree) {
            free(grid);
            return NULL;
        }

        grid->octree_root = tree->root;
        grid->active_count = 0;  /* No cells active initially */

        /* Store tree metadata (would need to keep Octree* in production) */
        /* For skeleton, we just keep the root pointer */
        free(tree);  /* Free container, keep root */
    }

    return grid;
}

/* ========================================================================
 * GRID DESTRUCTION
 * ======================================================================== */

void grid_destroy(Grid* grid) {
    if (!grid) {
        return;
    }

    if (grid->type == GRID_UNIFORM) {
        /* Free dense cell array */
        if (grid->cells) {
            free(grid->cells);
            grid->cells = NULL;
        }
    } else {
        /* Free sparse octree */
        if (grid->octree_root) {
            octree_free_node(grid->octree_root);
            grid->octree_root = NULL;
        }
    }

    free(grid);
}

/* ========================================================================
 * GRID ACCESS (Skeleton Implementation)
 * ======================================================================== */

GridCell_SAB* grid_get_cell(Grid* grid, int i, int j) {
    return grid_get_cell_3d(grid, i, j, 0);
}

GridCell_SAB* grid_get_cell_3d(Grid* grid, int i, int j, int k) {
    /* Bounds check */
    if (!grid || i < 0 || i >= grid->nx || j < 0 || j >= grid->ny || k < 0 || k >= grid->nz) {
        return NULL;
    }

    if (grid->type == GRID_UNIFORM) {
        /* Direct array access */
        if (!grid->cells) {
            return NULL;  /* Not allocated (skeleton) */
        }
        size_t idx = (size_t)k * (size_t)grid->nx * (size_t)grid->ny
                   + (size_t)j * (size_t)grid->nx
                   + (size_t)i;
        return &grid->cells[idx];

    } else {
        /* Sparse octree lookup (skeleton: always return NULL) */
        /* In production, would traverse octree to find cell */
        return NULL;
    }
}

GridCell_SAB* grid_activate_cell(Grid* grid, int i, int j) {
    if (!grid) {
        return NULL;
    }

    if (grid->type == GRID_UNIFORM) {
        /* All cells are always active in uniform grid */
        return grid_get_cell(grid, i, j);
    }

    /* Sparse grid: would allocate cell storage here */
    /* Skeleton: just return NULL */
    return NULL;
}

void grid_deactivate_cell(Grid* grid, int i, int j) {
    if (!grid || grid->type == GRID_UNIFORM) {
        return;  /* No-op for uniform grids */
    }

    /* Sparse grid: would deallocate cell storage here */
    /* Skeleton: no-op */
    (void)i;
    (void)j;
}

/* ========================================================================
 * GRID ITERATION (Skeleton Implementation)
 * ======================================================================== */

void grid_foreach_active(Grid* grid, GridCellCallback callback, void* user_data) {
    if (!grid || !callback) {
        return;
    }

    if (grid->type == GRID_UNIFORM && grid->cells) {
        /* Iterate all cells in dense array */
        for (int j = 0; j < grid->ny; j++) {
            for (int i = 0; i < grid->nx; i++) {
                GridCell_SAB* cell = grid_get_cell(grid, i, j);
                if (cell) {
                    callback(cell, i, j, user_data);
                }
            }
        }
    } else {
        /* Sparse: would iterate active cells in octree */
        /* Skeleton: no-op */
    }
}

/* ========================================================================
 * GRID UTILITIES
 * ======================================================================== */

size_t grid_memory_usage(const Grid* grid) {
    if (!grid) {
        return 0;
    }

    size_t base = sizeof(Grid);

    if (grid->type == GRID_UNIFORM) {
        /* Dense array size */
        return base + grid_total_cells(grid) * CELL_SIZE_ESTIMATE;
    } else {
        /* Sparse octree (skeleton estimate) */
        return base + grid->active_count * CELL_SIZE_ESTIMATE + 1024;  /* Overhead */
    }
}
