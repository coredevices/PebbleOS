/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   double floor(double x);

#include <pblibc_private.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// TODO: PBL-36144 replace this naive implementation with __builtin_floor()

#ifndef NAN
#define NAN         (0.0/0.0)
#endif

#ifndef INFINITY
#define INFINITY    (1.0/0.0)
#endif

#ifndef isinf
#define isinf(y) (__builtin_isinf(y))
#endif

#ifndef isnan
#define isnan(y) (__builtin_isnan(y))
#endif


double floor(double x) {
  if (isnan(x)) {
    return NAN;
  }
  if (isinf(x)) {
    return INFINITY;
  }

  const int64_t x_int = (int64_t)x;
  const bool has_no_fraction = (double)x_int == x;
  return (double)((x >= 0 || has_no_fraction) ? x_int : x_int - 1);
}
