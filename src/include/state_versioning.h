// state_versioning.h
#ifndef NEG_STATE_VERSIONING_H
#define NEG_STATE_VERSIONING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Increment when binary layout of SimulationState changes.
 * Use semantic version bumps when making incompatible changes.
 */
#define NEG_STATE_VERSION_MAJOR 1
#define NEG_STATE_VERSION_MINOR 0
#define NEG_STATE_VERSION_PATCH 0

/* Single u32 version token for easy comparisons in wire formats */
#define NEG_STATE_VERSION ((uint32_t)((NEG_STATE_VERSION_MAJOR << 16) | (NEG_STATE_VERSION_MINOR << 8) | (NEG_STATE_VERSION_PATCH)))

#ifdef __cplusplus
}
#endif

#endif /* NEG_STATE_VERSIONING_H */
