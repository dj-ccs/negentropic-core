/**
 * sparse_octree.c - Sparse Octree Allocation Skeleton (Genesis v3.0)
 *
 * Implements the Genesis v3.0 architectural principle #3:
 *   "Sparse is the Default Memory Model"
 *
 * This is a SKELETON IMPLEMENTATION providing only:
 *   - Root node allocation
 *   - Basic active cell list management
 *   - Memory budget tracking
 *
 * Full traversal, query, and GPU mapping are NOT implemented per
 * the Genesis v3.0 constraint: "Do not implement full sparse octree
 * traversal - only the allocation skeleton and grid-type switch."
 *
 * Author: negentropic-core team
 * Version: 0.4.0-alpha-genesis
 * Date: 2025-12-09
 * License: MIT OR GPL-3.0
 */

#include "sparse_octree.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/**
 * Allocate a single octree node with zero-initialized fields.
 */
static OctreeNode* alloc_node(void) {
    OctreeNode* node = (OctreeNode*)calloc(1, sizeof(OctreeNode));
    return node;
}

/* ========================================================================
 * ROOT ALLOCATION
 * ======================================================================== */

OctreeNode* octree_alloc_root(int nx, int ny, size_t budget_bytes) {
    /* Validate inputs */
    if (nx <= 0 || ny <= 0) {
        return NULL;
    }

    /* Allocate root node */
    OctreeNode* root = alloc_node();
    if (!root) {
        return NULL;
    }

    /* Initialize bounding box to cover entire grid */
    root->x0 = 0;
    root->y0 = 0;
    root->x1 = nx;
    root->y1 = ny;

    /* Root is at depth 0 */
    root->depth = 0;

    /* Initially a leaf (no children allocated) */
    root->is_leaf = 1;
    root->is_allocated = 1;

    /* No active cells yet */
    root->active_count = 0;
    root->active_indices = NULL;
    root->active_capacity = 0;

    /* Initialize children to NULL */
    for (int i = 0; i < OCTREE_CHILDREN; i++) {
        root->children[i] = NULL;
    }

    /* Budget is tracked at Octree level, not node level */
    (void)budget_bytes;

    return root;
}

Octree* octree_create(int nx, int ny, size_t budget_bytes) {
    /* Validate inputs */
    if (nx <= 0 || ny <= 0) {
        return NULL;
    }

    /* Allocate octree container */
    Octree* tree = (Octree*)calloc(1, sizeof(Octree));
    if (!tree) {
        return NULL;
    }

    /* Allocate root node */
    tree->root = octree_alloc_root(nx, ny, budget_bytes);
    if (!tree->root) {
        free(tree);
        return NULL;
    }

    /* Store grid dimensions */
    tree->nx = nx;
    tree->ny = ny;

    /* Initialize memory tracking */
    tree->memory_budget = budget_bytes;
    tree->memory_used = sizeof(Octree) + sizeof(OctreeNode);

    /* Initialize statistics */
    tree->total_nodes = 1;
    tree->leaf_nodes = 1;
    tree->total_active = 0;

    return tree;
}

/* ========================================================================
 * DEALLOCATION
 * ======================================================================== */

void octree_free_node(OctreeNode* node) {
    if (!node) {
        return;
    }

    /* Recursively free children */
    for (int i = 0; i < OCTREE_CHILDREN; i++) {
        if (node->children[i]) {
            octree_free_node(node->children[i]);
            node->children[i] = NULL;
        }
    }

    /* Free active indices array */
    if (node->active_indices) {
        free(node->active_indices);
        node->active_indices = NULL;
    }

    /* Free the node itself */
    free(node);
}

void octree_destroy(Octree* tree) {
    if (!tree) {
        return;
    }

    /* Free the root and all descendants */
    octree_free_node(tree->root);
    tree->root = NULL;

    /* Free the container */
    free(tree);
}

/* ========================================================================
 * ACTIVE CELL MANAGEMENT (Skeleton Implementation)
 * ======================================================================== */

/**
 * Grow the active indices array if needed.
 */
static int ensure_active_capacity(OctreeNode* node, uint32_t needed) {
    if (node->active_capacity >= needed) {
        return 0;  /* Already have enough space */
    }

    /* Grow by doubling, minimum 16 */
    uint32_t new_capacity = node->active_capacity * 2;
    if (new_capacity < 16) new_capacity = 16;
    if (new_capacity < needed) new_capacity = needed;

    /* Reallocate */
    uint32_t* new_indices = (uint32_t*)realloc(
        node->active_indices,
        new_capacity * sizeof(uint32_t)
    );
    if (!new_indices) {
        return -1;  /* Allocation failed */
    }

    node->active_indices = new_indices;
    node->active_capacity = new_capacity;

    return 0;
}

int octree_activate_cell(Octree* tree, int i, int j) {
    /* Validate inputs */
    if (!tree || !tree->root) {
        return -1;
    }
    if (i < 0 || i >= tree->nx || j < 0 || j >= tree->ny) {
        return -1;  /* Out of bounds */
    }

    /* For skeleton implementation, we just add to root's active list */
    OctreeNode* node = tree->root;

    /* Compute linear index */
    uint32_t idx = octree_linear_index(tree->nx, i, j);

    /* Check if already active (simple linear search for skeleton) */
    for (uint32_t k = 0; k < node->active_count; k++) {
        if (node->active_indices[k] == idx) {
            return 0;  /* Already active */
        }
    }

    /* Check memory budget */
    size_t needed_memory = tree->memory_used + sizeof(uint32_t);
    if (needed_memory > tree->memory_budget) {
        return -1;  /* Would exceed budget */
    }

    /* Ensure capacity */
    if (ensure_active_capacity(node, node->active_count + 1) != 0) {
        return -1;  /* Allocation failed */
    }

    /* Add to active list */
    node->active_indices[node->active_count] = idx;
    node->active_count++;

    /* Update statistics */
    tree->total_active++;
    tree->memory_used = needed_memory;

    return 0;
}

int octree_deactivate_cell(Octree* tree, int i, int j) {
    /* Validate inputs */
    if (!tree || !tree->root) {
        return -1;
    }
    if (i < 0 || i >= tree->nx || j < 0 || j >= tree->ny) {
        return -1;
    }

    OctreeNode* node = tree->root;
    uint32_t idx = octree_linear_index(tree->nx, i, j);

    /* Find and remove from active list (simple linear search) */
    for (uint32_t k = 0; k < node->active_count; k++) {
        if (node->active_indices[k] == idx) {
            /* Swap with last element and decrement count */
            node->active_indices[k] = node->active_indices[node->active_count - 1];
            node->active_count--;
            tree->total_active--;
            return 0;
        }
    }

    return -1;  /* Not found */
}

int octree_is_active(const Octree* tree, int i, int j) {
    if (!tree || !tree->root) {
        return 0;
    }
    if (i < 0 || i >= tree->nx || j < 0 || j >= tree->ny) {
        return 0;
    }

    const OctreeNode* node = tree->root;
    uint32_t idx = octree_linear_index(tree->nx, i, j);

    /* Linear search in active list */
    for (uint32_t k = 0; k < node->active_count; k++) {
        if (node->active_indices[k] == idx) {
            return 1;
        }
    }

    return 0;
}

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

size_t octree_memory_usage(const Octree* tree) {
    if (!tree) {
        return 0;
    }
    return tree->memory_used;
}
