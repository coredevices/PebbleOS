/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   void *memcpy(void *s1, const void *s2, size_t n);
//   void *memmove(void *s1, const void *s2, size_t n);
///////////////////////////////////////

#include <stddef.h>
#include <stdint.h>

void *memcpy(void * restrict s1, const void * restrict s2, size_t n) {
  char *dest = (char*)s1;
  const char *src = (const char*)s2;
  while (n--) {
    *dest++ = *src++;
  }
  return s1;
}

void *memmove(void * restrict s1, const void * restrict s2, size_t n) {
  char *dest = (char*)s1;
  const char *src = (const char*)s2;
  if (dest <= src) {
    while (n--) {
      *dest++ = *src++;
    }
  } else {
    while (n--) {
      dest[n] = src[n];
    }
  }
  return s1;
}
