/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   int memcmp(const void *s1, const void *s2, size_t n);

#include <stddef.h>

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char*)s1;
  const unsigned char *p2 = (const unsigned char*)s2;
  while (n--) {
    int diff = *p1 - *p2;
    if (diff) {
      return diff;
    }
    p1++;
    p2++;
  }
  return 0;
}
