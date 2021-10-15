// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file xpt.h
 *
 * Cross platform types and definitions.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * Defines fixed-size integer types and other  general utility types and
 * other macros that are ought to be available and equivalent across any
 * supported platform.
 *
 * This header keeps its dependencies at a bare minimum, and **MUST NOT**
 * excessively pollute the namespace.
 *
 * Including this header makes symbols from `stddef.h` and `stdint.h` visible.
 */

#ifndef DF_XPT_H_
#define DF_XPT_H_

#include <stddef.h>
#include <stdint.h>

// Extern function declarations for inline functions on header

/**
 * \def EXTERNC
 *
 * Expands to `extern` or `extern "C"` depending on whether or not the compiler
 * supports C++.
 *
 * This is useful to declare `extern` C functions inside inline functions
 * code within .h files, in order to avoid a full fledged `#include`.
 */
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC extern
#endif

// Boolean and fixed size types

typedef int Boolean;      ///< A boolean value, legal values are `TRUE` or `FALSE`
typedef int ToggleState;  ///< Switch toggle state, legal values are `ON` or `OFF`
typedef int Judgement;    ///< Operation result outcome, legal values are `OK` or `NG`

/// Boolean values (`true`, `false`).
enum BooleanLogic {
	FALSE = 0,  ///< `false` boolean value, guaranteed to be 0.
	TRUE  = 1   ///< `true` boolean value, guaranteed to be 1.
};

/// Switch toggle states (`on`, `off`).
enum BooleanSwitch {
	OFF = 0,  ///< `off` value, guaranteed to be 0.
	ON  = 1   ///< `on` value, guaranteed to be 1.
};

/// Operation success states (success, failure).
enum BooleanJudgement {
	OK =  0,  ///< Success state, guaranteed to be 0 (no errors).
	NG = -1   ///< Failure state, negative value, equals to -1 (no-good).
};

typedef char Boolean8;  ///< Packed boolean type, may hold the same values as `Boolean`

typedef int8_t    Sint8;    ///< Fixed-size signed 8-bits integer type
typedef uint8_t   Uint8;    ///< Fixed-size unsigned 8-bits integer type
typedef int16_t   Sint16;   ///< Fixed-size signed 16-bits integer type
typedef uint16_t  Uint16;   ///< Fixed-size unsigned 16-bits integer type
typedef int32_t   Sint32;   ///< Fixed-size signed 32-bits integer type
typedef uint32_t  Uint32;   ///< Fixed-size unsigned 32-bits integer type
typedef int64_t   Sint64;   ///< Fixed-size signed 64-bits integer type
typedef uint64_t  Uint64;   ///< Fixed-size unsigned 64-bits integer type
typedef intptr_t  Sintptr;  ///< Fixed-size signed type large enough to store a pointer
typedef uintptr_t Uintptr;  ///< Fixed-size unsigned type large enough to store a pointer

// Fixed size compile time constants

#define S8_C(c)  INT8_C(c)    ///< Expands constant `c` to a signed 8 bit integer literal
#define U8_C(c)  UINT8_C(c)   ///< Expands constant `c` to an unsigned 8 bit integer literal
#define S16_C(c) INT16_C(c)   ///< Expands constant `c` to a signed 16 bit integer literal
#define U16_C(c) UINT16_C(c)  ///< Expands constant `c` to an unsigned 16 bit integer literal
#define S32_C(c) INT32_C(c)   ///< Expands constant `c` to a signed 32 bit integer literal
#define U32_C(c) UINT32_C(c)  ///< Expands constant `c` to an unsigned 32 bit integer literal
#define S64_C(c) INT64_C(c)   ///< Expands constant `c` to a signed 64 bit integer literal
#define U64_C(c) UINT64_C(c)  ///< Expands constant `c` to an unsigned 64 bit integer literal

// Compiler support for thread-locals, over/under-alignment,
// inline and no-return functions

// NOTE: we don't support TinyCC because there's no TLS support there

#ifdef __TINYC__
#error "Sorry, no TLS support available on Tiny C Compiler"
#endif

/**
 * \def THREAD_LOCAL
 *
 * \brief Declares a static variable as thread local.
 *
 * \def ALIGNED(a, what)
 *
 * \brief Force type, variable, or member alignment.
 *
 * \param a    Required alignment, must be a positive power of two and a compile-time integer literal.
 * \param what What should be aligned, a variable, type or member
 *
 * \def NORETURN
 *
 * \brief Declare a function shall never return to its caller (like `exit()`).
 *
 * \warning Behavior is undefined if function does return.
 *
 * \def UNREACHABLE
 *
 * \brief Mark code as unreachable.
 *
 * \warning Behavior is undefined if code is actually reached during execution.
 *
 * \def FORCE_INLINE
 *
 * \brief Require function inlining, regardless of compiler policy.
 *
 * \note Function may be inlined even in debug builds.
 *
 * \def INLINE
 *
 * \brief Suggest a function should be inlined, compiler may still choose not to.
 *
 * \def NOINLINE
 *
 * \brief Force the compiler **not** to inline a function,
 *        regardless of its own policy.
 */

#ifdef _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif
#ifdef _MSC_VER
#define ALIGNED(a, what) __declspec(align(a)) what
#else
#define ALIGNED(a, what) what __attribute__((__aligned__(a)))
#endif
#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#else
#define NORETURN __attribute__((__noreturn__))
#endif
#ifdef _MSC_VER
#define UNREACHABLE __assume(0)
#elif defined(__GNUC__)
#define UNREACHABLE __builtin_unreachable()
#else
#define UNREACHABLE ((void) 0) /*NOTREACHED*/
#endif
#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#define INLINE       __inline
#define NOINLINE     __declspec(noinline)
#elif defined(__GNUC__)
#define FORCE_INLINE static __inline__ __attribute__((__always_inline__, __unused__))
#define INLINE       static __inline__ __attribute__((__unused__))
#define NOINLINE     __attribute__((__noinline__))
#else
// this is a crude fallback and actually loses quite a bit of the intended semantics...
#if 0
#error "No FORCE_INLINE and NOINLINE for this platform"
#endif

#define FORCE_INLINE  static inline
#define INLINE        static inline
#define NOINLINE
#endif

// Compiler checks

/**
 * \def CHECK_PRINTF
 *
 * \brief Compiler check on `printf()`-like function arguments.
 *
 * If compiler supports it, check at compile-time that `printf()` and `vprintf()`
 * like formatted message arguments are valid.
 *
 * \param f `const char *` message template argument index (1 is first argument)
 * \param a Variadic arguments start (`...` argument index), use 0 if function takes a `va_list` (1 is first argument)
 *
 * \def STATIC_ASSERT
 *
 * \brief Compile time assertion.
 *
 * If compiler supports it, test a condition at compile time, aborting
 * compilation on failure.
 *
 * \param what Expression to be evaluated at compile time
 * \param msg  Failure message
 */

#ifdef __GNUC__
#define CHECK_PRINTF(f, a) __attribute__((__format__(__printf__, f, a)))
#else
#define CHECK_PRINTF(f, a)
#endif

#ifndef __cplusplus
#define STATIC_ASSERT(what, msg) _Static_assert(what, msg)
#else
#define STATIC_ASSERT(what, msg) static_assert(what, msg)
#endif

// NOTE: if we ever need packing just use the MSVC style #pragma pack(push, N)
// + #pragma pack(pop), it's pretty much universal.

// Platform suitable memory alignment type

/**
 * \def ALIGNMENT
 *
 * \brief Platform memory alignment required for primitive types.
 *
 * \note This alignment is only safe for primitive and trivial struct types,
 *       but may be insufficient for SIMD types or overaligned types.
 */
#ifdef _MSC_VER
#ifdef _WIN32
#define ALIGNMENT 4
#else
#define ALIGNMENT 8
#endif
#elif defined(__GNUC__)
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
// conservative guess
#define ALIGNMENT 16
#endif

/**
 * \brief Perform or test alignment of size `x` or pointer `p` to `align` bytes.
 *
 * \param x     Unsigned integral size in bytes
 * \param p     A pointer
 * \param align Unsigned integral power of two specifying alignment
 *
 * \warning `align` must be a power of 2, macro arguments are evaluated more
 *          than once.
 *
 * @{
 *   \def ALIGN
 *   \def ALIGN_DOWN
 *   \def ALIGN_PTR
 *   \def ALIGN_PTR_DOWN
 *
 *   \def IS_ALIGNED
 *   \def IS_PTR_ALIGNED
 * @}
 */
#define ALIGN(x, align) \
	(((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) \
	ALIGN((x) - ((align) - 1), align)
#define ALIGN_PTR(p, align) \
	((void *) ALIGN((Uintptr) (p), align))
#define ALIGN_PTR_DOWN(p, align) \
	((void *) ALIGN_DOWN((Uintptr) (p), align))

#define IS_ALIGNED(x, align)     (((x) & ((align) - 1)) == 0)
#define IS_PTR_ALIGNED(p, align) IS_ALIGNED((Uintptr) p, align)

/**
 * \def alloca
 *
 * \brief Dynamic memory allocation on stack.
 *
 * \param size Size to be allocated, in bytes, unsigned integral value
 *
 * \warning Allocating large chunks of memory on stack is a sure one-way ticket
 *          to a stack overflow, there is no way to indicate memory allocation
 *          failures for `alloca()`.
 */
#ifdef _MSC_VER
#define alloca(size) _alloca(size)
#else
#define alloca(size) __builtin_alloca(size)
#endif

/**
 * \def ARRAY_SIZE
 *
 * \brief Array element count.
 *
 * \param array Array to be sized
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif
/**
 * \def BIT
 *
 * \brief Return unsigned integer constant with a single bit set.
 *
 * \param idx Bit index to be set (0 is LSB)
 */
#ifndef BIT
#define BIT(idx) (1uLL << (idx))
#endif
/**
 * \def USED
 *
 * Suppress unused variable warnings for `x`.
 *
 * \param x Identifier to mark as used
 */
#ifndef USED
#define USED(x) ((void) (x))
#endif

/**
 * \def EDN_LE
 *
 * \brief Constant identifying little-endian byte ordering (LSB first).
 *
 * \def EDN_BE
 *
 * \brief Constant identifying big-endian byte ordering (MSB first).
 *
 * \def EDN_NATIVE
 *
 * \brief Constant identifying native byte ordering,
 *        equals either `EDN_BE` or `EDN_LE`.
 */
#ifdef _WIN32

#define EDN_LE     0
#define EDN_BE     1
#define EDN_NATIVE EDN_LE

#else

#define EDN_LE     __ORDER_LITTLE_ENDIAN__
#define EDN_BE     __ORDER_BIG_ENDIAN__
#define EDN_NATIVE __BYTE_ORDER__

#endif

#if EDN_NATIVE != EDN_LE && EDN_NATIVE != EDN_BE
#error "Unsupported platform endianness"
#endif

/**
 * \brief Swaps bytes inside 16, 32 and 64 bits unsigned integers.
 *
 * Truncates `x` to the required amount of bytes, if necessary,
 * and reverses each byte around. The value is then returned.
 * If `x` is a compile-time constant the result is also a constant.
 *
 * \param x Unsigned integer value to be byte-swapped
 *
 * \warning `x` is evaluated multiple times.
 *
 * @{
 *   \def BSWAP16
 *   \def BSWAP32
 *   \def BSWAP64
 * @}
 */
#ifndef BSWAP16
#define BSWAP16(x) ((Uint16) ( \
	(((x) & 0xff00u) >> 8) | (((x) & 0x00ffu) << 8) \
))
#endif
#ifndef BSWAP32
#define BSWAP32(x) ((Uint32) ( \
	(((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >> 8) | \
	(((x) & 0x0000ff00u) << 8)  | (((x) & 0x000000ffu) << 24)  \
))
#endif
#ifndef BSWAP64
#define BSWAP64(x) ((Uint64) ( \
	(((x) & 0xff00000000000000uLL) >> 56) | \
	(((x) & 0x00ff000000000000uLL) >> 40) | \
	(((x) & 0x0000ff0000000000uLL) >> 24) | \
	(((x) & 0x000000ff00000000uLL) >> 8)  | \
	(((x) & 0x00000000ff000000uLL) << 8)  | \
	(((x) & 0x0000000000ff0000uLL) << 24) | \
	(((x) & 0x000000000000ff00uLL) << 40) | \
	(((x) & 0x00000000000000ffuLL) << 56)   \
))
#endif

/**
 * \brief Declare a fixed-size big or little endian constant.
 *
 * Following macros may be used to declare fixed size
 * constant of a specific endianness at compile time.
 * Original value is swapped if destination endianness
 * is not match the host endianness. These macros are usable for
 * variables as well, but functions declared in `sys/endian.h` should
 * be preferred whenever possible (i.e. use these macros on variables
 * only inside headers to reduce `#include`s).
 *
 * \param x Constant or variable, which is truncated to the required size.
 *
 * \return `x` as a constant or variable byte-swapped as needed.
 *
 * \warning `x` is evaluated multiple times.
 *
 * @{
 *   \def BE16
 *   \def BE32
 *   \def BE64
 *   \def LE16
 *   \def LE32
 *   \def LE64
 * @}
 */
#if EDN_NATIVE == EDN_LE
#define BE16(x) BSWAP16(x)
#define BE32(x) BSWAP32(x)
#define BE64(x) BSWAP64(x)
#define LE16(x) ((Uint16) (x))
#define LE32(x) ((Uint32) (x))
#define LE64(x) ((Uint64) (x))
#else
#define BE16(x) ((Uint16) (x))
#define BE32(x) ((Uint32) (x))
#define BE64(x) ((Uint64) (x))
#define LE16(x) BSWAP16(x)
#define LE32(x) BSWAP32(x)
#define LE64(x) BSWAP64(x)
#endif

/**
 * \brief Minimum, maximum and clamped to range values.
 *
 * \param x, y Generic comparable values
 * \param a, b Clamping range, inclusive, `a` must be less than or equal to `b`
 *
 * \warning Arguments are evaluated more than once.
 *
 * @{
 *   \def MIN
 *   \def MAX
 *   \def CLAMP
 * @}
 */
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) (((x) < (y)) ? (y) : (x))
#endif
#ifndef CLAMP
#define CLAMP(x, a, b) MAX(a, MIN(x, b))
#endif

/**
 * \def ABS
 *
 * \brief Absolute value of `x` using ternary `?:` operator.
 *
 * \param x An expression comparable with 0
 *
 * \warning `x` is evaluated more than once.
 */
#ifndef ABS
#define ABS(x) (((x) >= 0) ? (x) : -(x))
#endif

/**
 * \def FLEX_ARRAY
 *
 * \brief Clarity macro to convey that a trailing array member inside
 *        a `struct` is an array of arbitrary size.
 */
#ifdef __cplusplus
#define FLEX_ARRAY 1
#elif defined(_MSC_VER) || defined(__GNUC__)
#define FLEX_ARRAY
#else
#define FLEX_ARRAY 1
#endif

#endif

