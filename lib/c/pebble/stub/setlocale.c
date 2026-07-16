/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   char *setlocale(int category, const char *locale);
///////////////////////////////////////
// Notes:
//   Simplicity references this, but we don't want to implement it.

#include <stddef.h>
char *setlocale(int category, const char *locale) {
  return NULL;
}
