/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   void *memset(void *s, int c, size_t n);
///////////////////////////////////////

#include <stddef.h>

void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char*)s;
  while (n--) {
    *p++ = (unsigned char)c;
  }
  return s;
}
