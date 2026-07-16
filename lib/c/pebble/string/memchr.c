/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   void *memchr(const void *s, int c, size_t n);

#include <stddef.h>
#include <pblibc_private.h>

void *memchr(const void *s, int c, size_t n) {
  const unsigned char *p = (const unsigned char*)s;
  unsigned char ch = (unsigned char)c;
  for (size_t i = 0; i < n; i++) {
    if (p[i] == ch) {
      return (void*)&p[i];
    }
  }
  return NULL;
}
