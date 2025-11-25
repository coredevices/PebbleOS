/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   size_t strlen(const char *s);
//   size_t strnlen(const char *s, size_t maxlen);
///////////////////////////////////////
// Exports to apps:
//   strlen

#include <stddef.h>
#include <pblibc_private.h>

size_t strlen(const char *s) {
  size_t len = 0;
  while (*s++) {
    len++;
  }
  return len;
}

size_t strnlen(const char *s, size_t maxlen) {
  size_t len = 0;
  while (*s++ && maxlen--) {
    len++;
  }
  return len;
}
