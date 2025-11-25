/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_LIBC_ASSERT_H
#define JERRY_LIBC_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef NDEBUG
#define assert(x) \
  do \
  { \
    if (__builtin_expect (!(x), 0)) \
    { \
      fprintf (stderr, "%s:%d: %s: Assertion `%s' failed.", __FILE__, __LINE__, __func__, #x); \
      abort (); \
    } \
  } while (0)
#else /* NDEBUG */
#define assert(x) ((void) 0)
#endif /* !NDEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRY_LIBC_ASSERT_H */
