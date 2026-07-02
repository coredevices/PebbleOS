/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   char *strchr(const char *s1, int c);
//   char *strrchr(const char *s1, int c);
///////////////////////////////////////
// Notes:
//   Tuned for code size.

#include <stddef.h>
#include <string.h>
#include <pblibc_private.h>

char *strchr(const char *s, int c) {
  size_t n = strlen(s) + 1;
  return memchr(s, c, n);
}

char *strrchr(const char *s, int c) {
  char ch = (char)c;
  size_t i = strlen(s);
  do {
    if (s[i] == ch) {
      return (char*)&s[i];
    }
  } while (i--);
  return NULL;
}
