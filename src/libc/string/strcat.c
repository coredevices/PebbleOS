/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   char *strcat(char *s1, const char *s2);
//   char *strncat(char *s1, const char *s2, size_t n);
///////////////////////////////////////
// Exports to apps:
//   strcat, strncat
///////////////////////////////////////
// Notes:
//   Tuned for code size.

#include <string.h>
#include <pblibc_private.h>

char *strcat(char * restrict s1, const char * restrict s2) {
  char *rc = s1;
  s1 += strlen(s1);
  strcpy(s1, s2);
  return rc;
}

char *strncat(char * restrict s1, const char * restrict s2, size_t n) {
  char * rc = s1;
  s1 += strlen(s1);

  size_t strn = strlen(s2);
  if (strn > n) {
    strn = n;
  }
  memcpy(s1, s2, strn);
  s1[strn] = '\0';
  return rc;
}
