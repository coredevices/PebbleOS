/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "console/dbgserial.h"
#include "system/die.h"
#include "system/reboot_reason.h"
#include "system/status_codes.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "pbl/kernel/compiler.h"

#define SPLIT_64_BIT_ARG(x) (uint32_t)((x >> 32) & 0xFFFFFFFF), (uint32_t)(x & 0xFFFFFFFF)

#define LOG_BUFFER_LENGTH 128

// Minimum amount of stack space required for vsniprintf
#define LOGGING_MIN_STACK_FOR_SPRINTF 240
#define LOGGING_STACK_FULL_MSG        ((const char *)" [STK FULL]")

void pbl_log_hashed_async(const uint32_t packed_loghash, ...);

void pbl_log_hashed_sync(const uint32_t packed_loghash, ...);

// Core Number must be shifted to the correct position.
void pbl_log_hashed_core(const uint32_t core_number, const uint32_t packed_loghash, ...);

// Core Number must be shifted to the correct position.
void pbl_log_hashed_vargs(const bool async, const uint32_t core_number,
                          const uint32_t packed_loghash, va_list fmt_args);

void pbl_log_vargs(uint8_t log_level, const char *src_filename, int src_line_number,
                   const char *fmt, va_list args);

void pbl_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt, ...)
    PBL_FORMAT_PRINTF(4, 5);

void pbl_log_sync(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
                  ...) PBL_FORMAT_PRINTF(4, 5);

int pbl_log_binary_format(char *buffer, int buffer_len, const uint8_t log_level,
                          const char *src_filename_path, int src_line_number, const char *fmt,
                          va_list args);

int pbl_log_get_bin_format(char *buffer, int buffer_len, const uint8_t log_level,
                           const char *src_filename_path, int src_line_number, const char *fmt,
                           ...);

#define LOG_LEVEL_ALWAYS        0
#define LOG_LEVEL_ERROR         1
#define LOG_LEVEL_WARNING       50
#define LOG_LEVEL_INFO          100
#define LOG_LEVEL_DEBUG         200
#define LOG_LEVEL_DEBUG_VERBOSE 255

// Module level that disables every log in the module
#define LOG_LEVEL_OFF (-1)

#ifdef CONFIG_DEFAULT_LOG_LEVEL
#define DEFAULT_LOG_LEVEL CONFIG_DEFAULT_LOG_LEVEL
#else
#define DEFAULT_LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#if defined(CONFIG_FLASH_LOG_LEVEL_ERROR)
#define FLASH_LOG_LEVEL LOG_LEVEL_ERROR
#elif defined(CONFIG_FLASH_LOG_LEVEL_WARNING)
#define FLASH_LOG_LEVEL LOG_LEVEL_WARNING
#elif defined(CONFIG_FLASH_LOG_LEVEL_DEBUG)
#define FLASH_LOG_LEVEL LOG_LEVEL_DEBUG
#elif defined(CONFIG_FLASH_LOG_LEVEL_DEBUG_VERBOSE)
#define FLASH_LOG_LEVEL LOG_LEVEL_DEBUG_VERBOSE
#else
#define FLASH_LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifdef CONFIG_LOG_HASHED
#include <pbl/logging/log_hashing.h>
#endif

#define LOG_COLOR_BLACK   "BLACK" // Not so useful in general
#define LOG_COLOR_RED     "RED"
#define LOG_COLOR_GREEN   "GREEN"
#define LOG_COLOR_YELLOW  "YELLOW"
#define LOG_COLOR_BLUE    "BLUE"
#define LOG_COLOR_MAGENTA "MAGENTA"
#define LOG_COLOR_CYAN    "CYAN"
#define LOG_COLOR_GREY    "GREY"
// Reserved for bold. Use sparingly
#define LOG_COLOR_LIGHT_GREY    "LIGHT_GREY"
#define LOG_COLOR_LIGHT_RED     "LIGHT_RED"
#define LOG_COLOR_LIGHT_GREEN   "LIGHT_GREEN"
#define LOG_COLOR_LIGHT_YELLOW  "LIGHT_YELLOW"
#define LOG_COLOR_LIGHT_BLUE    "LIGHT_BLUE"
#define LOG_COLOR_LIGHT_MAGENTA "LIGHT_MAGENTA"
#define LOG_COLOR_LIGHT_CYAN    "LIGHT_CYAN"
#define LOG_COLOR_WHITE         "WHITE"

// Level-to-color mapping (fixed per level)
#define LOG_COLOR_FOR_ALWAYS  LOG_COLOR_BLUE
#define LOG_COLOR_FOR_ERROR   LOG_COLOR_RED
#define LOG_COLOR_FOR_WARNING LOG_COLOR_YELLOW
#define LOG_COLOR_FOR_INFO    LOG_COLOR_GREEN
#define LOG_COLOR_FOR_DEBUG   LOG_COLOR_GREY
#define LOG_COLOR_FOR_VERBOSE LOG_COLOR_GREY

#ifndef STRINGIFY
#define STRINGIFY_NX(a) #a
#define STRINGIFY(a)    STRINGIFY_NX(a)
#endif // STRINGIFY

#define STATUS_STRING(s) STRINGIFY(s)

// Per-module compile-time log level and name. PBL_LOG_MODULE_DEFINE /
// PBL_LOG_MODULE_DECLARE override these tentative definitions, e.g.
// PBL_LOG_MODULE_DEFINE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL) (see
// Kconfig.template.log_level). Kconfig never yields LOG_LEVEL_ALWAYS (0) for a
// module, so 0 marks a file without one; those use DEFAULT_LOG_LEVEL.
PBL_UNUSED static const int16_t _pbl_log_module_level;
PBL_UNUSED static const char *const _pbl_log_module_name;
// Runtime level of modules with CONFIG_<module>_LOG_LEVEL_RUNTIME, NULL otherwise.
PBL_UNUSED static int16_t *const _pbl_log_module_runtime_level;

#define _PBL_LOG_PLACEHOLDER_1            0,
#define _PBL_LOG_IS_ENABLED(cfg)          _PBL_LOG_IS_ENABLED1(cfg)
#define _PBL_LOG_IS_ENABLED1(val)         _PBL_LOG_IS_ENABLED2(_PBL_LOG_PLACEHOLDER_##val)
#define _PBL_LOG_IS_ENABLED2(arg)         _PBL_LOG_IS_ENABLED3(arg 1, 0)
#define _PBL_LOG_IS_ENABLED3(_, val, ...) val

#define _PBL_LOG_CAT(a, b)  _PBL_LOG_CAT_(a, b)
#define _PBL_LOG_CAT_(a, b) a##b

#ifdef CONFIG_LOG_HASHED
// The MODULE map entry gives the loghash dict generator the
// file -> module mapping; the module name costs nothing at runtime.
#define _PBL_LOG_MODULE_STATIC(name, level)                                                   \
  PBL_UNUSED static const int16_t _pbl_log_module_level = (level);                            \
  PBL_UNUSED static const char *const _pbl_log_module_name = #name;                           \
  PBL_USED PBL_NOCOMMON PBL_SECTION(".log_strings") static const char _pbl_log_module_map[] = \
      "MODULE:" __FILE__ ":" #name
#else
#define _PBL_LOG_MODULE_STATIC(name, level)                        \
  PBL_UNUSED static const int16_t _pbl_log_module_level = (level); \
  PBL_UNUSED static const char *const _pbl_log_module_name = #name
#endif

// Runtime modules keep every message and filter on a shared level variable,
// initialized to the configured level.
#define _PBL_LOG_MODULE_DEFINE_0(name, level) _PBL_LOG_MODULE_STATIC(name, level)
#define _PBL_LOG_MODULE_DEFINE_1(name, level)      \
  int16_t _pbl_log_runtime_level_##name = (level); \
  _PBL_LOG_MODULE_DECLARE_1(name, level)

#define _PBL_LOG_MODULE_DECLARE_0(name, level) _PBL_LOG_MODULE_STATIC(name, level)
#define _PBL_LOG_MODULE_DECLARE_1(name, level)                                                     \
  extern int16_t _pbl_log_runtime_level_##name;                                                    \
  PBL_UNUSED static int16_t *const _pbl_log_module_runtime_level = &_pbl_log_runtime_level_##name; \
  _PBL_LOG_MODULE_STATIC(name, LOG_LEVEL_DEBUG_VERBOSE)

// Unit tests build with CONFIG_LOG but without the board Kconfig symbols,
// so module levels fall back to the default there.
#if defined(CONFIG_LOG) && !defined(UNITTEST)
#define PBL_LOG_MODULE_DEFINE(name, level) \
  _PBL_LOG_CAT(_PBL_LOG_MODULE_DEFINE_, _PBL_LOG_IS_ENABLED(level##_RUNTIME))(name, level)
#define PBL_LOG_MODULE_DECLARE(name, level) \
  _PBL_LOG_CAT(_PBL_LOG_MODULE_DECLARE_, _PBL_LOG_IS_ENABLED(level##_RUNTIME))(name, level)
#else
#define PBL_LOG_MODULE_DEFINE(name, level) PBL_UNUSED static const int16_t _pbl_log_module_level = 0
#define PBL_LOG_MODULE_DECLARE(name, level) PBL_LOG_MODULE_DEFINE(name, level)
#endif

//! Get/set the level of a module built with CONFIG_<module>_LOG_LEVEL_RUNTIME.
#define PBL_LOG_MODULE_LEVEL_GET(name)                                 \
  ({                                                                   \
    extern int16_t _pbl_log_runtime_level_##name;                      \
    __atomic_load_n(&_pbl_log_runtime_level_##name, __ATOMIC_RELAXED); \
  })
#define PBL_LOG_MODULE_LEVEL_SET(name, level)                                    \
  do {                                                                           \
    extern int16_t _pbl_log_runtime_level_##name;                                \
    __atomic_store_n(&_pbl_log_runtime_level_##name, (level), __ATOMIC_RELAXED); \
  } while (0)

#define PBL_SHOULD_LOG(level)                                                             \
  ((level) <= (_pbl_log_module_level != 0 ? _pbl_log_module_level : DEFAULT_LOG_LEVEL) && \
   (_pbl_log_module_runtime_level == NULL ||                                              \
    (level) <= __atomic_load_n(_pbl_log_module_runtime_level, __ATOMIC_RELAXED)))

// Internal implementation macros (use level-named macros below instead)
#ifdef CONFIG_LOG
#ifdef CONFIG_LOG_HASHED
#define PBL_LOG_COLOR(level, color, fmt, ...)                               \
  do {                                                                      \
    if (PBL_SHOULD_LOG(level)) {                                            \
      NEW_LOG_HASH(pbl_log_hashed_async, level, color, fmt, ##__VA_ARGS__); \
    }                                                                       \
  } while (0)

#define PBL_LOG_COLOR_SYNC(level, color, fmt, ...)                         \
  do {                                                                     \
    if (PBL_SHOULD_LOG(level)) {                                           \
      NEW_LOG_HASH(pbl_log_hashed_sync, level, color, fmt, ##__VA_ARGS__); \
    }                                                                      \
  } while (0)
#else
#define PBL_LOG_COLOR(level, color, fmt, ...)                                                \
  do {                                                                                       \
    if (PBL_SHOULD_LOG(level)) {                                                             \
      if (_pbl_log_module_name != NULL) {                                                    \
        pbl_log(level, __FILE__, __LINE__, "%s: " fmt, _pbl_log_module_name, ##__VA_ARGS__); \
      } else {                                                                               \
        pbl_log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__);                              \
      }                                                                                      \
    }                                                                                        \
  } while (0)

#define PBL_LOG_COLOR_SYNC(level, color, fmt, ...)                                                \
  do {                                                                                            \
    if (PBL_SHOULD_LOG(level)) {                                                                  \
      if (_pbl_log_module_name != NULL) {                                                         \
        pbl_log_sync(level, __FILE__, __LINE__, "%s: " fmt, _pbl_log_module_name, ##__VA_ARGS__); \
      } else {                                                                                    \
        pbl_log_sync(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__);                              \
      }                                                                                           \
    }                                                                                             \
  } while (0)
#endif
#else // !CONFIG_LOG
#define PBL_LOG_COLOR(level, color, fmt, ...)
#define PBL_LOG_COLOR_SYNC(level, color, fmt, ...)
#endif // CONFIG_LOG

// Level-named macros (async)
#define PBL_LOG_ALWAYS(fmt, ...) \
  PBL_LOG_COLOR(LOG_LEVEL_ALWAYS, LOG_COLOR_FOR_ALWAYS, fmt, ##__VA_ARGS__)
#define PBL_LOG_ERR(fmt, ...) \
  PBL_LOG_COLOR(LOG_LEVEL_ERROR, LOG_COLOR_FOR_ERROR, fmt, ##__VA_ARGS__)
#define PBL_LOG_WRN(fmt, ...) \
  PBL_LOG_COLOR(LOG_LEVEL_WARNING, LOG_COLOR_FOR_WARNING, fmt, ##__VA_ARGS__)
#define PBL_LOG_INFO(fmt, ...) PBL_LOG_COLOR(LOG_LEVEL_INFO, LOG_COLOR_FOR_INFO, fmt, ##__VA_ARGS__)
#define PBL_LOG_DBG(fmt, ...) \
  PBL_LOG_COLOR(LOG_LEVEL_DEBUG, LOG_COLOR_FOR_DEBUG, fmt, ##__VA_ARGS__)
#define PBL_LOG_VERBOSE(fmt, ...) \
  PBL_LOG_COLOR(LOG_LEVEL_DEBUG_VERBOSE, LOG_COLOR_FOR_VERBOSE, fmt, ##__VA_ARGS__)

// Level-named sync macros
#define PBL_LOG_SYNC_ALWAYS(fmt, ...) \
  PBL_LOG_COLOR_SYNC(LOG_LEVEL_ALWAYS, LOG_COLOR_FOR_ALWAYS, fmt, ##__VA_ARGS__)
#define PBL_LOG_SYNC_ERR(fmt, ...) \
  PBL_LOG_COLOR_SYNC(LOG_LEVEL_ERROR, LOG_COLOR_FOR_ERROR, fmt, ##__VA_ARGS__)
#define PBL_LOG_SYNC_WRN(fmt, ...) \
  PBL_LOG_COLOR_SYNC(LOG_LEVEL_WARNING, LOG_COLOR_FOR_WARNING, fmt, ##__VA_ARGS__)
#define PBL_LOG_SYNC_INFO(fmt, ...) \
  PBL_LOG_COLOR_SYNC(LOG_LEVEL_INFO, LOG_COLOR_FOR_INFO, fmt, ##__VA_ARGS__)
#define PBL_LOG_SYNC_DBG(fmt, ...) \
  PBL_LOG_COLOR_SYNC(LOG_LEVEL_DEBUG, LOG_COLOR_FOR_DEBUG, fmt, ##__VA_ARGS__)
#define PBL_LOG_SYNC_VERBOSE(fmt, ...) \
  PBL_LOG_COLOR_SYNC(LOG_LEVEL_DEBUG_VERBOSE, LOG_COLOR_FOR_VERBOSE, fmt, ##__VA_ARGS__)

#ifdef CONFIG_LOG
#define RETURN_STATUS(st)           \
  do {                              \
    if (FAILED(st)) {               \
      PBL_LOG_WRN("%d", (int)(st)); \
    }                               \
    return st;                      \
  } while (0)

#define RETURN_STATUS_UP(st) return ((st) != E_INVALID_ARGUMENT ? (st) : E_INTERNAL)
#else // CONFIG_LOG
#define RETURN_STATUS(st)    return (st)
#define RETURN_STATUS_UP(st) return ((st) == E_INVALID_ARGUMENT ? E_INTERNAL : (st))
#endif // CONFIG_LOG
