/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Horrible hack to get glibc's ctype.h to define isascii et al.
// Some of the weird options that are set in the unit test build environment
// force certain files to be included into every compilation unit before the
// first line of the source file. One of these implicitly-included headers
// includes stdint.h, which causes the feature macros to be interpreted before
// the compilation unit has a chance to set any feature macros. To work around
// this, the features.h header guard needs to be undefined so that the feature
// macros get reexamined for the next libc include. Ugh.
#undef _FEATURES_H
#define _DEFAULT_SOURCE  // Required for glibc 2.20+
#define _BSD_SOURCE
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "clar.h"

#define CTYPE_THEIRS(CHK) \
int CHK##_theirs(int c) { \
  return CHK(c); \
}
CTYPE_THEIRS(isalpha)
CTYPE_THEIRS(isupper)
CTYPE_THEIRS(islower)
CTYPE_THEIRS(isdigit)
CTYPE_THEIRS(isxdigit)
CTYPE_THEIRS(isspace)
CTYPE_THEIRS(ispunct)
CTYPE_THEIRS(isalnum)
CTYPE_THEIRS(isprint)
CTYPE_THEIRS(isgraph)
CTYPE_THEIRS(iscntrl)
CTYPE_THEIRS(isascii)

CTYPE_THEIRS(toascii)

// We have to do this because glibc throws specification to the wind.
// Quoting from the C99 spec:
//   If the argument is a character for which isupper is true and there are one or more
//   corresponding characters, as specified by the current locale, for which islower is true,
//   the tolower function returns one of the corresponding characters (always the same one
//   for any given locale); otherwise, the argument is returned unchanged.
// Notably, glibc will mangle inputs that are not hitting isupper/islower. Thanks Obama.
int toupper_theirs(int c) {
  if (islower(c)) {
    return toupper(c);
  } else {
    return c;
  }
}
int tolower_theirs(int c) {
  if (isupper(c)) {
    return tolower(c);
  } else {
    return c;
  }
}

// "Define" libc functions we're testing
#include "pblibc_private.h"
#include "include/ctype.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

#define ISCTYPE_TEST(CHK) \
  for(int i = -128; i < 256; i++) { \
    cl_assert_equal_b(CHK##_theirs(i), CHK(i)); \
  } \
  /* validate double-evaluation behavior */ \
  for(int i = -128; i < 256;) { \
    int them = CHK##_theirs(i); \
    int us = CHK(i++); \
    cl_assert_equal_b(them, us); \
  }

void test_ctype__isalpha(void) {
ISCTYPE_TEST(isalpha)
}
void test_ctype__isupper(void) {
ISCTYPE_TEST(isupper)
}
void test_ctype__islower(void) {
ISCTYPE_TEST(islower)
}
void test_ctype__isdigit(void) {
ISCTYPE_TEST(isdigit)
}
void test_ctype__isxdigit(void) {
ISCTYPE_TEST(isxdigit)
}
void test_ctype__isspace(void) {
ISCTYPE_TEST(isspace)
}
void test_ctype__ispunct(void) {
ISCTYPE_TEST(ispunct)
}
void test_ctype__isalnum(void) {
ISCTYPE_TEST(isalnum)
}
void test_ctype__isprint(void) {
ISCTYPE_TEST(isprint)
}
void test_ctype__isgraph(void) {
ISCTYPE_TEST(isgraph)
}
void test_ctype__iscntrl(void) {
ISCTYPE_TEST(iscntrl)
}
void test_ctype__isascii(void) {
ISCTYPE_TEST(isascii)
}

#define TOCTYPE_TEST(CHK) \
  for(int i = -128; i < 256; i++) { \
    cl_assert_equal_i(CHK##_theirs(i), CHK(i)); \
  } \
  /* validate double-evaluation behavior */ \
  for(int i = -128; i < 256;) { \
    int them = CHK##_theirs(i); \
    int us = CHK(i++); \
    cl_assert_equal_i(them, us); \
  }

void test_ctype__toascii(void) {
TOCTYPE_TEST(toascii)
}
void test_ctype__toupper(void) {
TOCTYPE_TEST(toupper)
}
void test_ctype__tolower(void) {
TOCTYPE_TEST(tolower)
}
