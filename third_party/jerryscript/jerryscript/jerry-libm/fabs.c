/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jerry-libm-internal.h"

/* fabs(x) returns the absolute value of x.
 */

double
fabs (double x)
{
  __HI (x) &= 0x7fffffff;
  return x;
} /* fabs */
