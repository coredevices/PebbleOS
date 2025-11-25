/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <string.h>

#include "util/attributes.h"

// Newer than 4.7
#if (__GNUC__ == 4 && __GNUC_MINOR__ > 7) || (__GNUC__ >= 5) || __clang__
void __aeabi_memcpy(void *dest, const void *src, size_t n) {
  memcpy(dest, src, n);
}

void __aeabi_memmove(void * restrict s1, const void * restrict s2, size_t n) {
  memmove(s1, s2, n);
}

ALIAS("__aeabi_memcpy") void __aeabi_memcpy4(void *dest, const void *src, size_t n);
ALIAS("__aeabi_memcpy") void __aeabi_memcpy8(void *dest, const void *src, size_t n);

void __aeabi_memset(void *s, size_t n, int c) {
  memset(s, c, n);
}

void __aeabi_memclr(void *addr, size_t n) {
  memset(addr, 0, n);
}

ALIAS("__aeabi_memclr") void __aeabi_memclr4(void *s, size_t n);
ALIAS("__aeabi_memclr") void __aeabi_memclr8(void *s, size_t n);

#endif
