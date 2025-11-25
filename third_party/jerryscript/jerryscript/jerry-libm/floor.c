/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jerry-libm-internal.h"

/* floor(x)
 * Return x rounded toward -inf to integral value
 *
 * Method:
 *      Bit twiddling.
 *
 * Exception:
 *      Inexact flag raised if x not equal to floor(x).
 */

#define huge 1.0e300

double
floor (double x)
{
  int i0, i1, j0;
  unsigned i, j;

  i0 = __HI (x);
  i1 = __LO (x);
  j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
  if (j0 < 20)
  {
    if (j0 < 0) /* raise inexact if x != 0 */
    {
      if (huge + x > 0.0) /* return 0 * sign(x) if |x| < 1 */
      {
        if (i0 >= 0)
        {
          i0 = i1 = 0;
        }
        else if (((i0 & 0x7fffffff) | i1) != 0)
        {
          i0 = 0xbff00000;
          i1 = 0;
        }
      }
    }
    else
    {
      i = (0x000fffff) >> j0;
      if (((i0 & i) | i1) == 0) /* x is integral */
      {
        return x;
      }
      if (huge + x > 0.0) /* raise inexact flag */
      {
        if (i0 < 0)
        {
          i0 += (0x00100000) >> j0;
        }
        i0 &= (~i);
        i1 = 0;
      }
    }
  }
  else if (j0 > 51)
  {
    if (j0 == 0x400) /* inf or NaN */
    {
      return x + x;
    }
    else /* x is integral */
    {
      return x;
    }
  }
  else
  {
    i = ((unsigned) (0xffffffff)) >> (j0 - 20);
    if ((i1 & i) == 0) /* x is integral */
    {
      return x;
    }
    if (huge + x > 0.0) /* raise inexact flag */
    {
      if (i0 < 0)
      {
        if (j0 == 20)
        {
          i0 += 1;
        }
        else
        {
          j = i1 + (1 << (52 - j0));
          if (j < i1) /* got a carry */
          {
            i0 += 1;
          }
          i1 = j;
        }
      }
      i1 &= (~i);
    }
  }
  __HI (x) = i0;
  __LO (x) = i1;
  return x;
} /* floor */

#undef huge
