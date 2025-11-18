/**
 * workspace_slab.h - Doom Ethos Slab Allocator for Integrator Workspaces
 *
 * Pre-allocated memory pool for integrator workspaces with zero runtime malloc.
 * All memory is allocated once at initialization, then handed out on request.
 *
 * Design Principles (Doom Ethos):
 * - No runtime malloc/calloc in hot paths
 * - Fixed-size pool with compile-time capacity
 * - Fast allocation (O(1) with bitmap scan)
 * - Deterministic memory layout for cache efficiency
 * - Thread-safe for multi-worker architectures
 *
 * Memory Budget:
 * - Max workspaces: 16 (one per worker thread + 4 spare)
 * - IntegratorWorkspace: ~512 bytes each = 8 KB total
 * - ClebschWorkspace: ~2 KB each (includes LUT) = 32 KB total
 * - Total slab size: ~40 KB
 *
 * Author: ClaudeCode (v2.2 Doom Ethos Sprint)
 * Version: 1.0
 * Date: 2025-11-18
 */

#ifndef NEG_WORKSPACE_SLAB_H
#define NEG_WORKSPACE_SLAB_H

#include "workspace.h"
#include "clebsch.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SLAB CONFIGURATION
 * ======================================================================== */

/**
 * Maximum number of concurrent integrator workspaces.
 * This should be >= number of worker threads + buffer for async operations.
 */
#define MAX_INTEGRATOR_WORKSPACES 16

/**
 * Maximum number of concurrent Clebsch workspaces.
 * Typically fewer than general workspaces since only LoD≥2 needs Clebsch.
 */
#define MAX_CLEBSCH_WORKSPACES 8

/* ========================================================================
 * SLAB INITIALIZATION
 * ======================================================================== */

/**
 * Initialize workspace slab allocator.
 *
 * Pre-allocates all workspace memory in contiguous blocks.
 * Must be called once at startup before any workspace_slab_alloc_* calls.
 *
 * Thread safety: Not thread-safe. Call from main thread before worker spawn.
 * Memory: Allocates ~40 KB static data
 *
 * @return 0 on success, -1 on failure
 */
int workspace_slab_init(void);

/**
 * Shutdown workspace slab allocator.
 *
 * Frees all pre-allocated memory and resets state.
 * Should be called at application shutdown.
 *
 * @return 0 on success
 */
int workspace_slab_shutdown(void);

/**
 * Get slab allocator statistics (for monitoring).
 *
 * @param integrator_used Output: Number of integrator workspaces in use
 * @param clebsch_used Output: Number of Clebsch workspaces in use
 * @return 0 on success
 */
int workspace_slab_stats(int* integrator_used, int* clebsch_used);

/* ========================================================================
 * SLAB ALLOCATION (replaces malloc/calloc)
 * ======================================================================== */

/**
 * Allocate an integrator workspace from the slab pool.
 *
 * Replaces: IntegratorWorkspace* ws = calloc(1, sizeof(IntegratorWorkspace));
 *
 * Performance: O(1) bitmap scan + memset
 * Memory: Returns pointer to pre-allocated slab entry, NOT heap memory
 *
 * @return Pointer to workspace, or NULL if pool exhausted
 */
IntegratorWorkspace* workspace_slab_alloc_integrator(void);

/**
 * Allocate a Clebsch workspace from the slab pool.
 *
 * Replaces: ClebschWorkspace* ws = calloc(1, sizeof(ClebschWorkspace));
 *
 * Performance: O(1) bitmap scan + memset
 * Memory: Returns pointer to pre-allocated slab entry, NOT heap memory
 *
 * Note: Clebsch LUTs are shared across all workspaces (singleton pattern).
 *       Only the workspace scratch buffers are per-workspace.
 *
 * @return Pointer to workspace, or NULL if pool exhausted
 */
ClebschWorkspace* workspace_slab_alloc_clebsch(void);

/* ========================================================================
 * SLAB DEALLOCATION (replaces free)
 * ======================================================================== */

/**
 * Free an integrator workspace back to the slab pool.
 *
 * Replaces: free(ws);
 *
 * Performance: O(1) bitmap clear + optional memset
 * Thread safety: Uses atomic operations for bitmap management
 *
 * @param ws Workspace pointer (must be from workspace_slab_alloc_integrator)
 */
void workspace_slab_free_integrator(IntegratorWorkspace* ws);

/**
 * Free a Clebsch workspace back to the slab pool.
 *
 * Replaces: free(ws);
 *
 * Performance: O(1) bitmap clear + optional memset
 * Thread safety: Uses atomic operations for bitmap management
 *
 * @param ws Workspace pointer (must be from workspace_slab_alloc_clebsch)
 */
void workspace_slab_free_clebsch(ClebschWorkspace* ws);

/* ========================================================================
 * DEBUGGING & VALIDATION
 * ======================================================================== */

/**
 * Validate that a workspace pointer belongs to the slab pool.
 *
 * Useful for catching double-free or use-after-free bugs.
 *
 * @param ws Workspace pointer to validate
 * @return true if pointer is within slab bounds, false otherwise
 */
bool workspace_slab_validate_integrator(const IntegratorWorkspace* ws);
bool workspace_slab_validate_clebsch(const ClebschWorkspace* ws);

/**
 * Check if slab pool is exhausted (for warning logs).
 *
 * @return true if all workspaces are allocated
 */
bool workspace_slab_is_exhausted(void);

#ifdef __cplusplus
}
#endif

#endif /* NEG_WORKSPACE_SLAB_H */
