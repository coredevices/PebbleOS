/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#define _BSD_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>

#include "jerry-core/jerry-port.h"

#include "us_ticker_api.h"

/**
 * Provide console message implementation for the engine.
 */
void
jerry_port_console (const char *format, /**< format string */
                    ...) /**< parameters */
{
  va_list args;
  va_start (args, format);
  vfprintf (stdout, format, args);
  va_end (args);
} /* jerry_port_console */

/**
 * Provide log message implementation for the engine.
 */
void
jerry_port_log (jerry_log_level_t level, /**< log level */
                const char *format, /**< format string */
                ...)  /**< parameters */
{
  (void) level; /* ignore log level */

  va_list args;
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
} /* jerry_port_log */

/**
 * Implementation of jerry_port_fatal.
 */
void
jerry_port_fatal (jerry_fatal_code_t code) /**< fatal code enum item */
{
  exit (code);
} /* jerry_port_fatal */

/**
 * Implementation of jerry_port_get_time_zone.
 * 
 * @return true - if success
 */
bool
jerry_port_get_time_zone (jerry_time_zone_t *tz_p) /**< timezone pointer */
{
  tz_p->offset = 0;
  tz_p->daylight_saving_time = 0;
  return true;
} /* jerry_port_get_time_zone */

/**
 * Implementation of jerry_port_get_current_time.
 *
 * @return current timer's counter value in microseconds 
 */
double
jerry_port_get_current_time ()
{
  return (double) us_ticker_read ();
} /* jerry_port_get_current_time */
