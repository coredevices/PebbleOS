/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/dbgserial.h"
#include "system/die.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#define LOG_LEVEL_ALWAYS 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARNING 50
#define LOG_LEVEL_INFO 100
#define LOG_LEVEL_DEBUG 200
#define LOG_LEVEL_DEBUG_VERBOSE 255

#ifndef STRINGIFY
  #define STRINGIFY_NX(a) #a
  #define STRINGIFY(a) STRINGIFY_NX(a)
#endif // STRINGIFY

#define STATUS_STRING(s) STRINGIFY(s)

#ifdef PBL_LOG_ENABLED
  #define PBL_LOG(level, fmt, args...) \
    do { \
      char _pbl_log_buffer[128]; \
      dbgserial_putstr_fmt(_pbl_log_buffer, sizeof(_pbl_log_buffer), \
          __FILE_NAME__ ":" STRINGIFY(__LINE__) "> " fmt, ## args); \
    } while (0)

  #ifdef VERBOSE_LOGGING

    #define PBL_LOG_VERBOSE(fmt, args...)                               \
      PBL_LOG(LOG_LEVEL_DEBUG, fmt, ## args)

  #else // VERBOSE_LOGGING
    #define PBL_LOG_VERBOSE(fmt, args...)
  #endif // VERBOSE_LOGGING

#else // PBL_LOG_ENABLED
  #define PBL_LOG(level, fmt, args...)
  #define PBL_LOG_VERBOSE(fmt, args...)
#endif // PBL_LOG_ENABLED
