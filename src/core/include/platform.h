/*
 * platform.h - Cross-Platform Compatibility Macros
 *
 * Defines alignment macros and platform-specific helpers for ensuring
 * consistent behavior across different compilers and architectures.
 *
 * Author: negentropic-core team
 * Version: 0.1.0
 * License: MIT OR GPL-3.0
 */

#ifndef NEG_PLATFORM_H
#define NEG_PLATFORM_H

/* ========================================================================
 * ALIGNMENT MACROS
 * ======================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define NEG_ALIGN16 __attribute__((aligned(16)))
    #define NEG_ALIGN32 __attribute__((aligned(32)))
    #define NEG_ALIGN64 __attribute__((aligned(64)))
#elif defined(_MSC_VER)
    #define NEG_ALIGN16 __declspec(align(16))
    #define NEG_ALIGN32 __declspec(align(32))
    #define NEG_ALIGN64 __declspec(align(64))
#else
    #define NEG_ALIGN16
    #define NEG_ALIGN32
    #define NEG_ALIGN64
    #warning "Alignment attributes not supported on this compiler"
#endif

/* ========================================================================
 * PACKING MACROS
 * ======================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define NEG_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
    #define NEG_PACKED
    #pragma pack(push, 1)
#else
    #define NEG_PACKED
#endif

/* ========================================================================
 * INLINE MACROS
 * ======================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define NEG_INLINE static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define NEG_INLINE static __forceinline
#else
    #define NEG_INLINE static inline
#endif

/* ========================================================================
 * RESTRICT KEYWORD
 * ======================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define NEG_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define NEG_RESTRICT __restrict
#else
    #define NEG_RESTRICT
#endif

#endif /* NEG_PLATFORM_H */
