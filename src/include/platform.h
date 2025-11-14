// platform.h
#ifndef NEG_PLATFORM_H
#define NEG_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Alignment macros for SIMD-friendly and cross-platform memory layouts */
#if defined(_MSC_VER)
  #define NEG_ALIGN16 __declspec(align(16))
  #define NEG_ALIGN32 __declspec(align(32))
#else
  #define NEG_ALIGN16 __attribute__((aligned(16)))
  #define NEG_ALIGN32 __attribute__((aligned(32)))
#endif

/* Portable packed struct macro */
#if defined(_MSC_VER)
  #define NEG_PACKED
  #pragma pack(push, 1)
#else
  #define NEG_PACKED __attribute__((packed))
#endif

/* End packing (for MSVC) */
#if defined(_MSC_VER)
  #pragma pack(pop)
#endif

/* Helper macros */
#define NEG_UNUSED(x) ((void)(x))

/* End extern "C" */
#ifdef __cplusplus
}
#endif

#endif /* NEG_PLATFORM_H */
