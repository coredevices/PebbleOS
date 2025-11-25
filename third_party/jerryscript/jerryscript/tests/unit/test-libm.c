/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * Unit test for jerry-libm
 */

#include "test-common.h"

static bool passed = true;

static void
check_int (const char *expr, int computed, int expected)
{
  printf ("%s = %d [expected=%d] ", expr, computed, expected);

  bool result = computed == expected;
  printf ("%s\n", result ? "PASS" : "FAIL");

  passed &= result;
} /* check_int */

typedef union
{
  double value;
  uint64_t bits64;
  uint32_t bits32[2];
} double_bits_t;

static void
check_double (const char *expr, double computed, double expected)
{
  double_bits_t computed_bits;
  double_bits_t expected_bits;

  computed_bits.value = computed;
  expected_bits.value = expected;

  printf ("%s = 0x%08x%08x [expected=0x%08x%08x] ", expr,
          (unsigned int) computed_bits.bits32[1], (unsigned int) computed_bits.bits32[0],
          (unsigned int) expected_bits.bits32[1], (unsigned int) expected_bits.bits32[0]);

  bool result;
  if (isnan (computed) && isnan (expected))
  {
    result = true;
  }
  else
  {
    int64_t diff = (int64_t) (computed_bits.bits64 - expected_bits.bits64);
    if (diff < 0)
    {
      diff = -diff;
    }

    if (diff <= 1) /* tolerate 1 bit of differene in the last place */
    {
      result = true;
      if (diff != 0)
      {
        printf ("APPROX ");
      }
    }
    else
    {
      result = false;
    }
  }
  printf ("%s\n", result ? "PASS" : "FAIL");

  passed &= result;
} /* check_double */

int
main ()
{
#define INF INFINITY
#include "test-libm.inc.h"

  return passed ? 0 : 1;
} /* main */
