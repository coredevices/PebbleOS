/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test-common.h"

#define TEST_MAX_DEPTH 10
#define TEST_ITERATIONS_NUM 256

jmp_buf buffers[TEST_MAX_DEPTH];

static void
test_setjmp_longjmp (volatile int depth)
{
  if (depth != TEST_MAX_DEPTH)
  {
    int a = 1, b = 2, c = 3;

    int array[256];
    for (int i = 0; i < 256; i++)
    {
      array[i] = i;
    }

    (void) a;
    (void) b;
    (void) c;
    (void) array;

    int k = setjmp (buffers[depth]);

    if (k == 0)
    {
      test_setjmp_longjmp (depth + 1);
    }
    else
    {
      TEST_ASSERT (k == depth + 1);

      TEST_ASSERT (a == 1);
      TEST_ASSERT (b == 2);
      TEST_ASSERT (c == 3);

      for (int i = 0; i < 256; i++)
      {
        TEST_ASSERT (array[i] == i);
      }
    }
  }
  else
  {
    int t = rand () % depth;
    TEST_ASSERT (t >= 0 && t < depth);

    longjmp (buffers[t], t + 1);
  }
} /* test_setjmp_longjmp */

int
main ()
{
  TEST_INIT ();

  for (int i = 0; i < TEST_ITERATIONS_NUM; i++)
  {
    test_setjmp_longjmp (0);
  }

  return 0;
} /* main */
