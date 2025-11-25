/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   int memcmp(const void *s1, const void *s2, size_t n);

#include <stddef.h>
#include <stdint.h>
#include <pblibc_private.h>

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char*)s1;
  const unsigned char *p2 = (const unsigned char*)s2;

  // Fast path: word-by-word comparison if both pointers are word-aligned
  if ((((uintptr_t)p1 | (uintptr_t)p2) & (sizeof(uint32_t) - 1)) == 0) {
    const uint32_t *w1 = (const uint32_t*)p1;
    const uint32_t *w2 = (const uint32_t*)p2;

    while (n >= sizeof(uint32_t)) {
      if (*w1 != *w2) {
        break;
      }
      w1++;
      w2++;
      n -= sizeof(uint32_t);
    }

    p1 = (const unsigned char*)w1;
    p2 = (const unsigned char*)w2;
  }

  // Byte-by-byte comparison for remaining/unaligned bytes
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
