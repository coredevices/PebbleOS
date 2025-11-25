/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#include <sys/time.h>

#include "jerry-port.h"
#include "jerry-port-default.h"

/**
 * Default implementation of jerry_port_get_time_zone.
 */
bool jerry_port_get_time_zone (jerry_time_zone_t *tz_p)
{
  struct timeval tv;
  struct timezone tz;

  /* gettimeofday may not fill tz, so zero-initializing */
  tz.tz_minuteswest = 0;
  tz.tz_dsttime = 0;

  if (gettimeofday (&tv, &tz) != 0)
  {
    return false;
  }

  tz_p->offset = tz.tz_minuteswest;
  tz_p->daylight_saving_time = tz.tz_dsttime > 0 ? 1 : 0;

  return true;
} /* jerry_port_get_time_zone */

/**
 * Default implementation of jerry_port_get_current_time.
 */
double jerry_port_get_current_time ()
{
  struct timeval tv;

  if (gettimeofday (&tv, NULL) != 0)
  {
    return 0;
  }

  return ((double) tv.tv_sec) * 1000.0 + ((double) tv.tv_usec) / 1000.0;
} /* jerry_port_get_current_time */
