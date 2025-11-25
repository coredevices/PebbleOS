/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// There's no way to check this from the preprocessor, and this header isn't freestanding, so...
// If we ever upgrade GCC, we may need to re-evaluate these two.
// You can check the types by running `arm-none-eabi-gcc -dM -E - </dev/null` and looking for
// __UINT32_TYPE__ and __UINTPTR_TYPE__ respectively.
#define __UINT32_IS_LONG 1
#define __UINTPTR_IS_LONG 0

#define __PRICHAR ""
#define __PRISHORT ""
#define __PRIINT ""
#define __PRILONG "l"
#define __PRILLONG "ll"

#define __PRI8(t) __PRICHAR#t
#define __PRI16(t) __PRISHORT#t
#if __UINT32_IS_LONG
# define __PRI32(t) __PRILONG#t
#else
# define __PRI32(t) __PRIINT#t
#endif

#define __PRI64(t) __PRILLONG#t

#if __UINTPTR_IS_LONG
#define __PRIPTR(t) __PRILONG#t
#else
#define __PRIPTR(t) __PRIINT#t
#endif

#ifndef PRId8
# define PRId8 __PRI8(d)
#endif
#ifndef PRIi8
# define PRIi8 __PRI8(i)
#endif
#ifndef PRIo8
# define PRIo8 __PRI8(o)
#endif
#ifndef PRIu8
# define PRIu8 __PRI8(u)
#endif
#ifndef PRIx8
# define PRIx8 __PRI8(x)
#endif
#ifndef PRIX8
# define PRIX8 __PRI8(X)
#endif

#ifndef PRIdLEAST8
# define PRIdLEAST8 PRId8
#endif
#ifndef PRIdFAST8
# define PRIdFAST8 PRId8
#endif
#ifndef PRIiLEAST8
# define PRIiLEAST8 PRIi8
#endif
#ifndef PRIiFAST8
# define PRIiFAST8 PRIi8
#endif
#ifndef PRIoLEAST8
# define PRIoLEAST8 PRIo8
#endif
#ifndef PRIoFAST8
# define PRIoFAST8 PRIo8
#endif
#ifndef PRIuLEAST8
# define PRIuLEAST8 PRIu8
#endif
#ifndef PRIuFAST8
# define PRIuFAST8 PRIu8
#endif
#ifndef PRIxLEAST8
# define PRIxLEAST8 PRIx8
#endif
#ifndef PRIxFAST8
# define PRIxFAST8 PRIx8
#endif
#ifndef PRIXLEAST8
# define PRIXLEAST8 PRIX8
#endif
#ifndef PRIXFAST8
# define PRIXFAST8 PRIX8
#endif

#ifndef PRId16
# define PRId16 __PRI16(d)
#endif
#ifndef PRIi16
# define PRIi16 __PRI16(i)
#endif
#ifndef PRIo16
# define PRIo16 __PRI16(o)
#endif
#ifndef PRIu16
# define PRIu16 __PRI16(u)
#endif
#ifndef PRIx16
# define PRIx16 __PRI16(x)
#endif
#ifndef PRIX16
# define PRIX16 __PRI16(X)
#endif

#ifndef PRIdLEAST16
# define PRIdLEAST16 PRId16
#endif
#ifndef PRIdFAST16
# define PRIdFAST16 PRId16
#endif
#ifndef PRIiLEAST16
# define PRIiLEAST16 PRIi16
#endif
#ifndef PRIiFAST16
# define PRIiFAST16 PRIi16
#endif
#ifndef PRIoLEAST16
# define PRIoLEAST16 PRIo16
#endif
#ifndef PRIoFAST16
# define PRIoFAST16 PRIo16
#endif
#ifndef PRIuLEAST16
# define PRIuLEAST16 PRIu16
#endif
#ifndef PRIuFAST16
# define PRIuFAST16 PRIu16
#endif
#ifndef PRIxLEAST16
# define PRIxLEAST16 PRIx16
#endif
#ifndef PRIxFAST16
# define PRIxFAST16 PRIx16
#endif
#ifndef PRIXLEAST16
# define PRIXLEAST16 PRIX16
#endif
#ifndef PRIXFAST16
# define PRIXFAST16 PRIX16
#endif

#ifndef PRId32
# define PRId32 __PRI32(d)
#endif
#ifndef PRIi32
# define PRIi32 __PRI32(i)
#endif
#ifndef PRIo32
# define PRIo32 __PRI32(o)
#endif
#ifndef PRIu32
# define PRIu32 __PRI32(u)
#endif
#ifndef PRIx32
# define PRIx32 __PRI32(x)
#endif
#ifndef PRIX32
# define PRIX32 __PRI32(X)
#endif

#ifndef PRIdLEAST32
# define PRIdLEAST32 PRId32
#endif
#ifndef PRIdFAST32
# define PRIdFAST32 PRId32
#endif
#ifndef PRIiLEAST32
# define PRIiLEAST32 PRIi32
#endif
#ifndef PRIiFAST32
# define PRIiFAST32 PRIi32
#endif
#ifndef PRIoLEAST32
# define PRIoLEAST32 PRIo32
#endif
#ifndef PRIoFAST32
# define PRIoFAST32 PRIo32
#endif
#ifndef PRIuLEAST32
# define PRIuLEAST32 PRIu32
#endif
#ifndef PRIuFAST32
# define PRIuFAST32 PRIu32
#endif
#ifndef PRIxLEAST32
# define PRIxLEAST32 PRIx32
#endif
#ifndef PRIxFAST32
# define PRIxFAST32 PRIx32
#endif
#ifndef PRIXLEAST32
# define PRIXLEAST32 PRIX32
#endif
#ifndef PRIXFAST32
# define PRIXFAST32 PRIX32
#endif

#ifndef PRId64
# define PRId64 __PRI64(d)
#endif
#ifndef PRIi64
# define PRIi64 __PRI64(i)
#endif
#ifndef PRIo64
# define PRIo64 __PRI64(o)
#endif
#ifndef PRIu64
# define PRIu64 __PRI64(u)
#endif
#ifndef PRIx64
# define PRIx64 __PRI64(x)
#endif
#ifndef PRIX64
# define PRIX64 __PRI64(X)
#endif

#ifndef PRIdLEAST64
# define PRIdLEAST64 PRId64
#endif
#ifndef PRIdFAST64
# define PRIdFAST64 PRId64
#endif
#ifndef PRIiLEAST64
# define PRIiLEAST64 PRIi64
#endif
#ifndef PRIiFAST64
# define PRIiFAST64 PRIi64
#endif
#ifndef PRIoLEAST64
# define PRIoLEAST64 PRIo64
#endif
#ifndef PRIoFAST64
# define PRIoFAST64 PRIo64
#endif
#ifndef PRIuLEAST64
# define PRIuLEAST64 PRIu64
#endif
#ifndef PRIuFAST64
# define PRIuFAST64 PRIu64
#endif
#ifndef PRIxLEAST64
# define PRIxLEAST64 PRIx64
#endif
#ifndef PRIxFAST64
# define PRIxFAST64 PRIx64
#endif
#ifndef PRIXLEAST64
# define PRIXLEAST64 PRIX64
#endif
#ifndef PRIXFAST64
# define PRIXFAST64 PRIX64
#endif

#ifndef PRIdPTR
# define PRIdPTR __PRIPTR(d)
#endif
#ifndef PRIiPTR
# define PRIiPTR __PRIPTR(i)
#endif
#ifndef PRIoPTR
# define PRIoPTR __PRIPTR(o)
#endif
#ifndef PRIuPTR
# define PRIuPTR __PRIPTR(u)
#endif
#ifndef PRIxPTR
# define PRIxPTR __PRIPTR(x)
#endif
#ifndef PRIXPTR
# define PRIXPTR __PRIPTR(X)
#endif

#ifndef PRIdMAX
# define PRIdMAX PRId64
#endif
#ifndef PRIiMAX
# define PRIiMAX PRIi64
#endif
#ifndef PRIoMAX
# define PRIoMAX PRIo64
#endif
#ifndef PRIuMAX
# define PRIuMAX PRIu64
#endif
#ifndef PRIxMAX
# define PRIxMAX PRIx64
#endif
#ifndef PRIXMAX
# define PRIXMAX PRIX64
#endif
