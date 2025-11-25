/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   char *strstr(const char *s1, const char *s2);
///////////////////////////////////////
// Notes:
//   Tuned for code size.

#include <stddef.h>
#include <string.h>

char *strstr(const char *s1, const char *s2) {
  size_t len = strlen(s2);
  while (*s1) {
    if (strncmp(s1, s2, len) == 0) {
      return (char*)s1;
    }
    s1++;
  }
  return NULL;
}
