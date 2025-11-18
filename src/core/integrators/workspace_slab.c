/**
 * workspace_slab.c - Doom Ethos Slab Allocator Implementation
 *
 * Zero-malloc workspace pool for perfect determinism and cache efficiency.
 *
 * Author: ClaudeCode (v2.2 Doom Ethos Sprint)
 * Version: 1.0
 * Date: 2025-11-18
 */

#include "workspace_slab.h"
#include <string.h>
#include <stdatomic.h>

/* ========================================================================
 * SLAB STORAGE (static pre-allocated pools)
 * ======================================================================== */

/**
 * Integrator workspace pool (16 × 512 bytes = 8 KB)
 */
static IntegratorWorkspace integrator_pool[MAX_INTEGRATOR_WORKSPACES];

/**
 * Clebsch workspace pool (8 × 2 KB = 16 KB)
 */
static ClebschWorkspace clebsch_pool[MAX_CLEBSCH_WORKSPACES];

/**
 * Allocation bitmaps (1 bit per workspace)
 * 0 = free, 1 = allocated
 * Using atomic_uint for thread-safe allocation
 */
static atomic_uint integrator_bitmap = 0;
static atomic_uint clebsch_bitmap = 0;

/**
 * Singleton Clebsch LUT (shared across all Clebsch workspaces)
 * Allocated once at init, never freed
 */
static ClebschLUT shared_clebsch_lut;
static bool lut_initialized = false;

/**
 * Initialization flag
 */
static bool slab_initialized = false;

/* ========================================================================
 * SLAB INITIALIZATION
 * ======================================================================== */

int workspace_slab_init(void) {
    if (slab_initialized) {
        return 0;  // Already initialized
    }

    // Zero out all pools (ensures deterministic initial state)
    memset(integrator_pool, 0, sizeof(integrator_pool));
    memset(clebsch_pool, 0, sizeof(clebsch_pool));

    // Reset allocation bitmaps
    atomic_store(&integrator_bitmap, 0);
    atomic_store(&clebsch_bitmap, 0);

    // Initialize shared Clebsch LUT
    if (!lut_initialized) {
        int ret = clebsch_lut_init(&shared_clebsch_lut);
        if (ret != 0) {
            return -1;  // LUT initialization failed
        }
        lut_initialized = true;
    }

    slab_initialized = true;
    return 0;
}

int workspace_slab_shutdown(void) {
    if (!slab_initialized) {
        return 0;  // Not initialized, nothing to do
    }

    // Clear all pools (for clean shutdown)
    memset(integrator_pool, 0, sizeof(integrator_pool));
    memset(clebsch_pool, 0, sizeof(clebsch_pool));

    // Reset bitmaps
    atomic_store(&integrator_bitmap, 0);
    atomic_store(&clebsch_bitmap, 0);

    // Clean up Clebsch LUT
    if (lut_initialized) {
        clebsch_lut_cleanup(&shared_clebsch_lut);
        lut_initialized = false;
    }

    slab_initialized = false;
    return 0;
}

int workspace_slab_stats(int* integrator_used, int* clebsch_used) {
    if (!slab_initialized) {
        return -1;
    }

    // Count bits set in bitmaps
    unsigned int ibits = atomic_load(&integrator_bitmap);
    unsigned int cbits = atomic_load(&clebsch_bitmap);

    *integrator_used = __builtin_popcount(ibits);
    *clebsch_used = __builtin_popcount(cbits);

    return 0;
}

/* ========================================================================
 * SLAB ALLOCATION
 * ======================================================================== */

IntegratorWorkspace* workspace_slab_alloc_integrator(void) {
    if (!slab_initialized) {
        return NULL;  // Not initialized
    }

    // Atomic find-and-set first free slot
    while (true) {
        unsigned int bitmap = atomic_load(&integrator_bitmap);

        // Find first zero bit (free slot)
        int slot = -1;
        for (int i = 0; i < MAX_INTEGRATOR_WORKSPACES; i++) {
            if ((bitmap & (1u << i)) == 0) {
                slot = i;
                break;
            }
        }

        if (slot == -1) {
            // Pool exhausted
            return NULL;
        }

        // Try to atomically set this bit
        unsigned int new_bitmap = bitmap | (1u << slot);
        if (atomic_compare_exchange_weak(&integrator_bitmap, &bitmap, new_bitmap)) {
            // Success! We claimed this slot
            IntegratorWorkspace* ws = &integrator_pool[slot];
            memset(ws, 0, sizeof(IntegratorWorkspace));  // Zero for determinism
            return ws;
        }

        // CAS failed, another thread grabbed it. Retry.
    }
}

ClebschWorkspace* workspace_slab_alloc_clebsch(void) {
    if (!slab_initialized || !lut_initialized) {
        return NULL;  // Not initialized
    }

    // Atomic find-and-set first free slot
    while (true) {
        unsigned int bitmap = atomic_load(&clebsch_bitmap);

        // Find first zero bit (free slot)
        int slot = -1;
        for (int i = 0; i < MAX_CLEBSCH_WORKSPACES; i++) {
            if ((bitmap & (1u << i)) == 0) {
                slot = i;
                break;
            }
        }

        if (slot == -1) {
            // Pool exhausted
            return NULL;
        }

        // Try to atomically set this bit
        unsigned int new_bitmap = bitmap | (1u << slot);
        if (atomic_compare_exchange_weak(&clebsch_bitmap, &bitmap, new_bitmap)) {
            // Success! We claimed this slot
            ClebschWorkspace* ws = &clebsch_pool[slot];
            memset(ws, 0, sizeof(ClebschWorkspace));  // Zero for determinism

            // Attach shared LUT (singleton pattern)
            ws->lut = &shared_clebsch_lut;

            return ws;
        }

        // CAS failed, another thread grabbed it. Retry.
    }
}

/* ========================================================================
 * SLAB DEALLOCATION
 * ======================================================================== */

void workspace_slab_free_integrator(IntegratorWorkspace* ws) {
    if (!ws || !slab_initialized) {
        return;  // Invalid input
    }

    // Find which slot this workspace belongs to
    ptrdiff_t offset = ws - integrator_pool;
    if (offset < 0 || offset >= MAX_INTEGRATOR_WORKSPACES) {
        // Not from our pool! This is a bug.
        return;
    }

    int slot = (int)offset;

    // Atomically clear the bit
    while (true) {
        unsigned int bitmap = atomic_load(&integrator_bitmap);
        unsigned int new_bitmap = bitmap & ~(1u << slot);
        if (atomic_compare_exchange_weak(&integrator_bitmap, &bitmap, new_bitmap)) {
            break;  // Successfully freed
        }
    }

    // Optional: Zero the workspace for security (can be disabled for performance)
    memset(ws, 0, sizeof(IntegratorWorkspace));
}

void workspace_slab_free_clebsch(ClebschWorkspace* ws) {
    if (!ws || !slab_initialized) {
        return;  // Invalid input
    }

    // Find which slot this workspace belongs to
    ptrdiff_t offset = ws - clebsch_pool;
    if (offset < 0 || offset >= MAX_CLEBSCH_WORKSPACES) {
        // Not from our pool! This is a bug.
        return;
    }

    int slot = (int)offset;

    // Atomically clear the bit
    while (true) {
        unsigned int bitmap = atomic_load(&clebsch_bitmap);
        unsigned int new_bitmap = bitmap & ~(1u << slot);
        if (atomic_compare_exchange_weak(&clebsch_bitmap, &bitmap, new_bitmap)) {
            break;  // Successfully freed
        }
    }

    // Optional: Zero the workspace for security (can be disabled for performance)
    // Note: Don't clear ws->lut since it's a shared pointer
    ws->lut = NULL;  // Clear pointer for safety
    memset(ws, 0, sizeof(ClebschWorkspace));
}

/* ========================================================================
 * DEBUGGING & VALIDATION
 * ======================================================================== */

bool workspace_slab_validate_integrator(const IntegratorWorkspace* ws) {
    if (!ws || !slab_initialized) {
        return false;
    }

    ptrdiff_t offset = ws - integrator_pool;
    return (offset >= 0 && offset < MAX_INTEGRATOR_WORKSPACES);
}

bool workspace_slab_validate_clebsch(const ClebschWorkspace* ws) {
    if (!ws || !slab_initialized) {
        return false;
    }

    ptrdiff_t offset = ws - clebsch_pool;
    return (offset >= 0 && offset < MAX_CLEBSCH_WORKSPACES);
}

bool workspace_slab_is_exhausted(void) {
    if (!slab_initialized) {
        return true;  // Not initialized = can't allocate
    }

    unsigned int ibits = atomic_load(&integrator_bitmap);
    unsigned int cbits = atomic_load(&clebsch_bitmap);

    // Check if all bits are set
    bool integrator_full = (ibits == ((1u << MAX_INTEGRATOR_WORKSPACES) - 1));
    bool clebsch_full = (cbits == ((1u << MAX_CLEBSCH_WORKSPACES) - 1));

    return integrator_full || clebsch_full;
}
