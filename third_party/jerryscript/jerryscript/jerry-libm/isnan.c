/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jerry-libm-internal.h"

/* isnan(x) returns 1 is x is nan, else 0;
 * no branching!
 */

int
isnan (double x)
{
  int hx, lx;

  hx = (__HI (x) & 0x7fffffff);
  lx = __LO (x);
  hx |= (unsigned) (lx | (-lx)) >> 31;
  hx = 0x7ff00000 - hx;
  return ((unsigned) (hx)) >> 31;
} /* isnan */
