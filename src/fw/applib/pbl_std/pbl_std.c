/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/pbl_std/pbl_std.h"
#include "applib/app_logging.h"
#include "applib/applib_malloc.auto.h"
#include "process_state/app_state/app_state.h"
#include "process_state/worker_state/worker_state.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

// Time
time_t pbl_override_time(time_t *tloc) {
  time_t t = sys_get_time();
  if (tloc) {
    *tloc = t;
  }
  return (t);
}

// Manually construct double to avoid requiring soft-fp
static double prv_time_to_double(time_t time) {
  // time_t is 32bit signed int, convert it manually
#ifndef UNITTEST
  _Static_assert(sizeof(time_t) == 4, "Conversion depends on 32bit time_t");
#endif

  if (time == 0) {
    return 0;
  }

  // Construct the double in two uint32_t's then reinterpret as double
  // Using a uint64_t would pull in helper functions for 64bit shifts
  union {
    double result;
    uint32_t representation[2];
  } conv;

  conv.representation[1] = 0;
  if (time < 0) {
    // set the sign bit
    conv.representation[1] |= (1 << 31);

    // sign bit takes care of positive vs negative
    time *= -1;
  }

  uint32_t significand = (uint32_t)time;

  // In order to normalize the significand, we shift off all the leading 0s
  // plus the msb which is implicitly included
  int shift = __builtin_clz(time) + 1;

  significand <<= shift;

  // top 20 bits fit into the upper 32 bits of the double
  conv.representation[1] |= (significand >> 12);

  // bottom 12 bits go into the lower 32 bits of the double
  conv.representation[0] = (significand << 20);

  // Exponent is biased by 1023, then adjusted for the amount the significand was
  // shifted. Out of the 52 significand bits, the bottom 20 are never used.
  uint32_t exp = 1023 + 52 - shift - 20;
  // Set the exponent
  conv.representation[1] |= (exp << 20);

  return conv.result;
}

double pbl_override_difftime(time_t end, time_t beginning) {
  return prv_time_to_double(end - beginning);
}

time_t pbl_override_time_legacy(time_t *tloc) {
  time_t t = sys_get_time();
  time_t legacy_time = sys_time_utc_to_local(t);

  if (tloc) {
    *tloc = legacy_time;
  }
  return (legacy_time);
}

DEFINE_SYSCALL(time_t, pbl_override_mktime, struct tm *tb) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(tb, sizeof(struct tm));
  }
  return mktime(tb);
}

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  uint16_t t_ms;
  time_t t_s;
  sys_get_time_ms(&t_s, &t_ms);
  if (out_ms) {
    *out_ms = t_ms;
  }
  if (tloc) {
    *tloc = t_s;
  }
  return (t_ms);
}

uint16_t pbl_override_time_ms_legacy(time_t *tloc, uint16_t *out_ms) {
  uint16_t t_ms;
  time_t t_s;
  sys_get_time_ms(&t_s, &t_ms);
  time_t legacy_time = sys_time_utc_to_local(t_s);

  if (out_ms) {
    *out_ms = t_ms;
  }
  if (tloc) {
    *tloc = legacy_time;
  }
  return (t_ms);
}

extern size_t localized_strftime(char* s,
      size_t maxsize, const char* format, const struct tm* tim_p, const char *locale);


struct tm *pbl_override_gmtime(const time_t *timep) {
  struct tm *gmtime_tm = NULL;

  if (pebble_task_get_current() == PebbleTask_App) {
    gmtime_tm = app_state_get_gmtime_tm();
  } else {
    gmtime_tm = worker_state_get_gmtime_tm();
  }

  sys_gmtime_r(timep, gmtime_tm);
  return gmtime_tm;
}

struct tm *pbl_override_localtime(const time_t *timep) {
  struct tm *localtime_tm = NULL;
  char *localtime_zone = NULL;

  if (pebble_task_get_current() == PebbleTask_App) {
    localtime_tm = app_state_get_localtime_tm();
    localtime_zone = app_state_get_localtime_zone();
  } else {
    localtime_tm = worker_state_get_localtime_tm();
    localtime_zone = worker_state_get_localtime_zone();
  }

  sys_localtime_r(timep, localtime_tm);
  // We have to work around localtime_r resetting tm_zone below
  sys_copy_timezone_abbr((char*)localtime_zone, *timep);
  strncpy(localtime_tm->tm_zone, localtime_zone, TZ_LEN);

  return localtime_tm;
}


int pbl_strftime(char* s, size_t maxsize, const char* format, const struct tm* tim_p) {
  char *locale = app_state_get_locale_info()->app_locale_time;
  return sys_strftime(s, maxsize, format, tim_p, locale);
}

DEFINE_SYSCALL(int, sys_strftime, char* s, size_t maxsize, const char* format,
                                  const struct tm* tim_p, char *locale) {

  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(s, maxsize);
    syscall_assert_userspace_buffer(format, strlen(format));
    syscall_assert_userspace_buffer(tim_p, sizeof(struct tm));
  }

  return localized_strftime(s, maxsize, format, tim_p, locale);
}


void *pbl_memcpy(void *destination, const void *source, size_t num) {
  // In releases prior to FW 2.5 we used GCC 4.7 and newlib as our libc implementation. However,
  // in 2.5 we switched to GCC 4.8 and nano-newlib. We actually ran into bad apps that would pass
  // negative numbers as their num parameter, which newlib handled by copying nothing, but
  // newlib-nano didn't handle at all and would interpret size_t as a extremely large unsigned
  // number. Guard against this happening so apps that used to work with the old libc will work
  // with the new libc. See PBL-7873.
  if (((ptrdiff_t) num) <= 0) {
    return destination;
  }

  return memcpy(destination, source, num);
}

// Formatted printing. The ROM's libc library (newlib) is compiled without
// floating point support in order to save space. Because of this, you can
// get an exception and a OS reset if an app tries to use %f or any of the
// other floating point format types in an snprintf call. This is because the
// implementation does not advance the va_arg (pointer to arguments on the
// stack) pointer when it sees one of these FP types and will thus end up using
// that floating point argument as the argument for the *next* format type. If
// the next format type is a string for example, you end up with a bus error
// when it tries to use that argument as a pointer.
//
// To prevent an OS reset, we scan the format string here and bail with an
// APP_LOG message if we detect an attempt to use floating point. We also
// return a "floating point not supported" string in the passed in str
// buffer.
int pbl_snprintf(char * str, size_t n, const char * format, ...) {
  int ret;
  const char* fp_msg = "floating point not supported in snprintf";


  // Scan string and see if it has any floating point specifiers in it
  bool has_fp = false;
  bool end_spec;
  bool end_format=false;
  const char* fmt = format;
  char ch;

  while (true) {

    // Skip to next '%'
    while (*fmt != 0 && *fmt != '%')
      fmt++;
    if (*fmt == 0)
      break;
    fmt++;

    // Skip flags, width, and precision until we find the format
    //  specifier. If it's a floating point specifier, immediately bail.
    end_spec = false;
    while (!end_spec) {
      ch = *fmt++;
      switch (ch) {
        case 0:
          end_format = true;
          end_spec = true;
          break;

        // flags, precision, and width specifiers
        case '+':
        case ' ':
        case '-':
        case '#':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '.':
          break;

        // length specifiers
        case 'h':
        case 'l':
        case 'L':
        case 'j':
        case 'z':
        case 't':
          break;

        // floating point types
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
          has_fp = true;
          end_spec = true;
          break;

        default:
          end_spec = true;
          break;
      } // end of switch statement

    } // while (!end_spec);

    if (end_format || has_fp)
      break;
  } // while (true)

  // Return error message if we detected floating point
  if (has_fp) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Floating point is not supported by snprintf: "
        "it was called with format string '%s'", format);
    strncpy(str, fp_msg, n-1);
    str[n-1] = 0;
    return strlen(str);
  }

  // Safe to process
  va_list args;
  va_start(args, format);
  ret = vsnprintf(str, n, format, args);
  va_end(args);

  return ret;
}

