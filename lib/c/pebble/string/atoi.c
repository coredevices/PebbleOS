/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   int atoi(const char *nptr);
//   long int atol(const char *nptr);
///////////////////////////////////////
// Exports to apps:
//   atoi, atol

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <pblibc_private.h>

intmax_t strtoX_core(const char * restrict nptr, char ** restrict endptr, int base, bool do_errors,
                     intmax_t max, intmax_t min);

int atoi(const char *nptr) {
  return strtoX_core(nptr, NULL, 10, false, INT_MAX, INT_MIN);
}

long int atol(const char *nptr) {
  return strtoX_core(nptr, NULL, 10, false, INT_MAX, INT_MIN);
}
