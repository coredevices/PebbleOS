/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements: (sorta)
//  int isalpha(int c);
//  int isupper(int c);
//  int islower(int c);
//  int isdigit(int c);
//  int isxdigit(int c);
//  int isspace(int c);
//  int ispunct(int c);
//  int isalnum(int c);
//  int isprint(int c);
//  int isgraph(int c);
//  int iscntrl(int c);
//  int isascii(int c);

#if UNITTEST
# include <include/ctype.h>
#else
# include <ctype.h>
#endif

// Ordered for unsigned char
#if !UNITTEST
__attribute__((__externally_visible__))
#endif
const unsigned char __ctype_data[256] = {
  // ASCII set
  // These values can't change. Look at ctype.h for information on these macros.
// +00        +01        +02        +03        +04        +05        +06        +07
  _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT, // 00
  _CCT,      _CCT|_CSP, _CCT|_CSP, _CCT|_CSP, _CCT|_CSP, _CCT|_CSP, _CCT,      _CCT, // 08
  _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT, // 10
  _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT,      _CCT, // 18
  _CSP|_CPR, _CPU,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU, // 20
  _CPU,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU, // 28
  _CNU,      _CNU,      _CNU,      _CNU,      _CNU,      _CNU,      _CNU,      _CNU, // 30
  _CNU,      _CNU,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU, // 38
  _CPU,      _CUP|_CHX, _CUP|_CHX, _CUP|_CHX, _CUP|_CHX, _CUP|_CHX, _CUP|_CHX, _CUP, // 40
  _CUP,      _CUP,      _CUP,      _CUP,      _CUP,      _CUP,      _CUP,      _CUP, // 48
  _CUP,      _CUP,      _CUP,      _CUP,      _CUP,      _CUP,      _CUP,      _CUP, // 50
  _CUP,      _CUP,      _CUP,      _CPU,      _CPU,      _CPU,      _CPU,      _CPU, // 58
  _CPU,      _CLO|_CHX, _CLO|_CHX, _CLO|_CHX, _CLO|_CHX, _CLO|_CHX, _CLO|_CHX, _CLO, // 60
  _CLO,      _CLO,      _CLO,      _CLO,      _CLO,      _CLO,      _CLO,      _CLO, // 68
  _CLO,      _CLO,      _CLO,      _CLO,      _CLO,      _CLO,      _CLO,      _CLO, // 70
  _CLO,      _CLO,      _CLO,      _CPU,      _CPU,      _CPU,      _CPU,      _CCT, // 78
  // Non-ASCII set
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
};
