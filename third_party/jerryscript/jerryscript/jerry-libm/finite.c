/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jerry-libm-internal.h"

/* finite(x) returns 1 is x is finite, else 0;
 * no branching!
 */

int
finite (double x)
{
  int hx;

  hx = __HI (x);
  return (unsigned) ((hx & 0x7fffffff) - 0x7ff00000) >> 31;
} /* finite */
