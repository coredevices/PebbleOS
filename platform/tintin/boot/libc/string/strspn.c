/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   size_t strcspn(const char *s1, const char *s2);
//   size_t strspn(const char *s1, const char *s2);

#include <stddef.h>
#include <string.h>

size_t strcspn(const char *s1, const char *s2) {
  size_t len = 0;
  const char *p;
  while (s1[len]) {
    p = s2;
    while (*p) {
      if (s1[len] == *p++) {
        return len;
      }
    }
    len++;
  }
  return len;
}

size_t strspn(const char *s1, const char *s2) {
  size_t len = 0;
  const char *p;
  while (s1[len]) {
    p = s2;
    while (*p) {
      if (s1[len] == *p) {
        break;
      }
      p++;
    }
    if (!*p) {
      return len;
    }
    len++;
  }
  return len;
}
