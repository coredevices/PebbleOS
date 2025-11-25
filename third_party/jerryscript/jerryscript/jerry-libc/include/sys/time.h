/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_LIBC_TIME_H
#define JERRY_LIBC_TIME_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**
 * Time value structure
 */
struct timeval
{
  unsigned long tv_sec;   /**< seconds */
  unsigned long tv_usec;  /**< microseconds */
};

/**
 * Timezone structure
 */
struct timezone
{
  int tz_minuteswest;     /**< minutes west of Greenwich */
  int tz_dsttime;         /**< type of DST correction */
};

int gettimeofday (void *tp, void *tzp);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRY_LIBC_TIME_H */
