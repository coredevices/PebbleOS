/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "jerry-api.h"

#include "test-common.h"

/**
 * Unit test's main function.
 */
int
main ()
{
  TEST_INIT ();

  const jerry_char_t *strings[] =
  {
    (const jerry_char_t *) "1",
    (const jerry_char_t *) "0.5",
    (const jerry_char_t *) "12345",
    (const jerry_char_t *) "1e-45",
    (const jerry_char_t *) "-2.5e+38",
    (const jerry_char_t *) "-2.5e38",
    (const jerry_char_t *) "- 2.5e+38",
    (const jerry_char_t *) "-2 .5e+38",
    (const jerry_char_t *) "-2. 5e+38",
    (const jerry_char_t *) "-2.5e+ 38",
    (const jerry_char_t *) "-2.5 e+38",
    (const jerry_char_t *) "-2.5e +38",
    (const jerry_char_t *) "NaN",
    (const jerry_char_t *) "abc",
    (const jerry_char_t *) "   Infinity  ",
    (const jerry_char_t *) "-Infinity",
    (const jerry_char_t *) "0",
    (const jerry_char_t *) "0",
  };

  const ecma_number_t nums[] =
  {
    (ecma_number_t) 1.0,
    (ecma_number_t) 0.5,
    (ecma_number_t) 12345.0,
    (ecma_number_t) 1.0e-45,
    (ecma_number_t) -2.5e+38,
    (ecma_number_t) -2.5e+38,
    (ecma_number_t) NAN,
    (ecma_number_t) NAN,
    (ecma_number_t) NAN,
    (ecma_number_t) NAN,
    (ecma_number_t) NAN,
    (ecma_number_t) NAN,
    (ecma_number_t) NAN,
    (ecma_number_t) NAN,
    (ecma_number_t) INFINITY,
    (ecma_number_t) -INFINITY,
    (ecma_number_t) +0.0,
    (ecma_number_t) -0.0
  };

  for (uint32_t i = 0;
       i < sizeof (nums) / sizeof (nums[0]);
       i++)
  {
    ecma_number_t num = ecma_utf8_string_to_number (strings[i], lit_zt_utf8_string_size (strings[i]));

    if (num != nums[i]
        && (!ecma_number_is_nan (num)
            || !ecma_number_is_nan (nums[i])))
    {
      return 1;
    }
  }

  return 0;
} /* main */
