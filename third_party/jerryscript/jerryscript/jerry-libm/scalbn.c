/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jerry-libm-internal.h"

/* scalbn(x,n) returns x* 2**n  computed by  exponent
 * manipulation rather than by actually performing an
 * exponentiation or a multiplication.
 */

#define two54  1.80143985094819840000e+16 /* 0x43500000, 0x00000000 */
#define twom54 5.55111512312578270212e-17 /* 0x3C900000, 0x00000000 */
#define huge   1.0e+300
#define tiny   1.0e-300

double
scalbn (double x, int n)
{
  int k, hx, lx;

  hx = __HI (x);
  lx = __LO (x);
  k = (hx & 0x7ff00000) >> 20; /* extract exponent */
  if (k == 0) /* 0 or subnormal x */
  {
    if ((lx | (hx & 0x7fffffff)) == 0) /* +-0 */
    {
      return x;
    }
    x *= two54;
    hx = __HI (x);
    k = ((hx & 0x7ff00000) >> 20) - 54;
    if (n < -50000) /*underflow */
    {
      return tiny * x;
    }
  }
  if (k == 0x7ff) /* NaN or Inf */
  {
    return x + x;
  }
  k = k + n;
  if (k > 0x7fe) /* overflow  */
  {
    return huge * copysign (huge, x);
  }
  if (k > 0) /* normal result */
  {
    __HI (x) = (hx & 0x800fffff) | (k << 20);
    return x;
  }
  if (k <= -54)
  {
    if (n > 50000) /* in case integer overflow in n + k */
    {
      return huge * copysign (huge, x); /*overflow */
    }
    else
    {
      return tiny * copysign (tiny, x); /*underflow */
    }
  }
  k += 54; /* subnormal result */
  __HI (x) = (hx & 0x800fffff) | (k << 20);
  return x * twom54;
} /* scalbn */

#undef two54
#undef twom54
#undef huge
#undef tiny
