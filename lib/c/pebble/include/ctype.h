/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Casts to int ensure that the return value is int, as spec indicates

// Uppercase
#define _CUP (1 << 0)
// Lowercase
#define _CLO (1 << 1)
// Number
#define _CNU (1 << 2)
// Space
#define _CSP (1 << 3)
// Punctuation
#define _CPU (1 << 4)
// Control
#define _CCT (1 << 5)
// Printable (only for ' ')
#define _CPR (1 << 6)
// Hex character
#define _CHX (1 << 7)

// ctype flag table for all char values
extern const unsigned char __ctype_data[256];

// Cast to unsigned char to prevent overflow and to map signed char to our range.
// This lets us cut out 128 bytes of data from __ctype_data
#define __ctype_get(C) (__ctype_data[(unsigned char)(C)])
#define __ctype_check(C, FLG) ((int)(__ctype_get(C) & (FLG)))

#define isalpha(C)  __ctype_check(C, _CUP|_CLO)
#define isupper(C)  __ctype_check(C, _CUP)
#define islower(C)  __ctype_check(C, _CLO)
#define isdigit(C)  __ctype_check(C, _CNU)
#define isxdigit(C) __ctype_check(C, _CHX|_CNU)
#define isspace(C)  __ctype_check(C, _CSP)
#define ispunct(C)  __ctype_check(C, _CPU)
#define isalnum(C)  __ctype_check(C, _CUP|_CLO|_CNU)
#define isprint(C)  __ctype_check(C, _CUP|_CLO|_CNU|_CPU|_CPR)
#define isgraph(C)  __ctype_check(C, _CUP|_CLO|_CNU|_CPU)
#define iscntrl(C)  __ctype_check(C, _CCT)

#define isascii(C) ((int)(((unsigned char)(C)) < 0x80))
#define toascii(C) ((int)(((unsigned char)(C)) & 0x7F))

// __typeof__ use is to prevent double-evaluation
#define toupper(C) \
  ({ __typeof__(C) X = (C); \
      islower(X) ? (int)X - 'a' + 'A' : (int)X; })

#define tolower(C) \
  ({ __typeof__(C) X = (C); \
      isupper(X) ? (int)X - 'A' + 'a' : (int)X; })
