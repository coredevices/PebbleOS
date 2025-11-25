/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   int strcmp(const char *s1, const char *s2);
//   int strncmp(const char *s1, const char *s2, size_t n);
///////////////////////////////////////
// Exports to apps:
//   strcmp, strncmp
///////////////////////////////////////
// Notes:
//   Tuned for code size.

#include <stddef.h>
#include <string.h>
#include <pblibc_private.h>

int strcmp(const char *s1, const char *s2) {
  size_t n = strlen(s1) + 1;
  return memcmp(s1, s2, n);
}

int strncmp(const char *s1, const char *s2, size_t n) {
  size_t strn = strlen(s1) + 1;
  if (strn < n) {
    n = strn;
  }
  return memcmp(s1, s2, n);
}
