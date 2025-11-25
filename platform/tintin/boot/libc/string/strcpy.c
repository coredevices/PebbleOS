/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   char *strcpy(char *s1, const char *s2);
//   char *strncpy(char *s1, const char *s2, size_t n);
///////////////////////////////////////
// Notes:
//   Tuned for code size.

#include <stddef.h>
#include <string.h>

char *strcpy(char * restrict s1, const char * restrict s2) {
  size_t n = strlen(s2) + 1;
  return memcpy(s1, s2, n);
}

char *strncpy(char * restrict s1, const char * restrict s2, size_t n) {
  char *rc = s1;
  size_t strn = strlen(s2) + 1;
  if (strn > n) {
    strn = n;
  }
  memcpy(s1, s2, strn);
  if (n > strn) {
    memset(s1 + strn, '\0', n - strn);
  }
  return rc;
}
