// se3_types.h
#ifndef NEG_SE3_TYPES_H
#define NEG_SE3_TYPES_H

#include <stdint.h>
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-point base type for 16.16 format (Doom-style) */
typedef int32_t fxp16_16_t;

/* Double-precision SE(3) for oracle/exact calculations */
typedef struct {
    double R[9];    /* Row-major 3x3 rotation matrix (double) */
    double t[3];    /* Translation vector (meters) */
} SE3d;

/* Fixed-point SE(3) for embedded / deterministic runtime */
typedef struct {
    fxp16_16_t R[9];    /* 3x3 rotation matrix stored in 16.16 fixed */
    fxp16_16_t t[3];    /* translation (meters * FRACUNIT) */
} SE3fxp;

/* Size & alignment sanity macros */
#define SE3D_SIZE_BYTES (sizeof(SE3d))
#define SE3FXP_SIZE_BYTES (sizeof(SE3fxp))

#ifdef __cplusplus
}
#endif

#endif /* NEG_SE3_TYPES_H */
