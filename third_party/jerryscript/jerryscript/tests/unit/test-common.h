/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "jrt.h"

#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define TEST_ASSERT(x) \
  do \
  { \
    if (unlikely (!(x))) \
    { \
      jerry_port_log (JERRY_LOG_LEVEL_ERROR, \
                      "TEST: Assertion '%s' failed at %s(%s):%lu.\n", \
                      #x, \
                      __FILE__, \
                      __func__, \
                      (unsigned long) __LINE__); \
      jerry_fatal (ERR_FAILED_INTERNAL_ASSERTION); \
    } \
  } while (0)

/**
 * Test initialization statement that should be included
 * at the beginning of main function in every unit test.
 */
#define TEST_INIT() \
do \
{ \
  FILE *f_rnd = fopen ("/dev/urandom", "r"); \
 \
  if (f_rnd == NULL) \
  { \
    return 1; \
  } \
 \
  uint32_t seed; \
 \
  size_t bytes_read = fread (&seed, 1, sizeof (seed), f_rnd); \
 \
 fclose (f_rnd); \
 \
  if (bytes_read != sizeof (seed)) \
  { \
    return 1; \
  } \
 \
  srand (seed); \
} while (0)

#endif /* TEST_COMMON_H */
