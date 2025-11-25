/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   void *memset(void *s, int c, size_t n);
///////////////////////////////////////
// Exports to apps:
//   memset

#include <stddef.h>
#include <pblibc_assembly.h>
#include <pblibc_private.h>

#if !MEMSET_IMPLEMENTED_IN_ASSEMBLY
void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char*)s;
  while (n--) {
    *p++ = (unsigned char)c;
  }
  return s;
}
#endif
