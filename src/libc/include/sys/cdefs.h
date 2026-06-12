/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(__cplusplus)
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS };
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#define __P(protos) protos
#define __CONCAT(x, y) x ## y
#define __STRING(x) #x

/*
 * Compiler attribute shims expected by platform SDK/libc headers when they
 * are pulled in via #include_next chains (e.g. setjmp.h, assert.h).
 *
 * macOS SDK headers use __dead2, __cold, __disable_tail_calls.
 * Linux glibc headers use __THROW, __THROWNL, __LEAF, __NTH, __NTHNL.
 *
 * GCC/Clang support all underlying attributes, so define them
 * unconditionally.  On other compilers, expand to nothing.
 */
#if defined(__GNUC__) || defined(__clang__)

/* ---- macOS SDK macros --------------------------------------------------- */
# ifndef __dead2
#   define __dead2 __attribute__((__noreturn__))
# endif
# ifndef __cold
#   define __cold __attribute__((__cold__))
# endif
# ifndef __disable_tail_calls
#   if defined(__has_attribute) && __has_attribute(__disable_tail_calls__)
#     define __disable_tail_calls __attribute__((__disable_tail_calls__))
#   else
#     define __disable_tail_calls
#   endif
# endif

/* ---- Linux glibc macros ------------------------------------------------- */
# ifndef __LEAF
#   ifdef __GNUC__
#     define __LEAF , __leaf__
#   else
#     define __LEAF
#   endif
# endif
# ifndef __THROW
#   define __THROW __attribute__((__nothrow__ __LEAF))
# endif
# ifndef __THROWNL
#   define __THROWNL __attribute__((__nothrow__))
# endif
# ifndef __NTH
#   define __NTH(fct) __attribute__((__nothrow__ __LEAF)) fct
# endif
# ifndef __NTHNL
#   define __NTHNL(fct) __attribute__((__nothrow__)) fct
# endif

#else /* not GCC/Clang */

# ifndef __dead2
#   define __dead2
# endif
# ifndef __cold
#   define __cold
# endif
# ifndef __disable_tail_calls
#   define __disable_tail_calls
# endif
# ifndef __THROW
#   define __THROW
# endif
# ifndef __THROWNL
#   define __THROWNL
# endif
# ifndef __NTH
#   define __NTH(fct) fct
# endif
# ifndef __NTHNL
#   define __NTHNL(fct) fct
# endif

#endif /* GCC/Clang */
