/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jerry-libm-internal.h"

/* copysign(x,y) returns a value with the magnitude of x and
 * with the sign bit of y.
 */

double
copysign (double x, double y)
{
  __HI (x) = (__HI (x) & 0x7fffffff) | (__HI (y) & 0x80000000);
  return x;
} /* copysign */
