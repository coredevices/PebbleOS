/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Logging is compiled out. Arguments stay type-checked in dead code so values
// only used for logging do not trigger unused warnings.

#pragma once

static inline void z_log_nop(const char *fmt, ...) {
  (void)fmt;
}

static inline void z_log_hexdump_nop(const void *data, unsigned int len, const char *str) {
  (void)data;
  (void)len;
  (void)str;
}

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR  1
#define LOG_LEVEL_WRN  2
#define LOG_LEVEL_INF  3
#define LOG_LEVEL_DBG  4

#define LOG_MODULE_REGISTER(...) extern int z_log_module_unused
#define LOG_MODULE_DECLARE(...)  extern int z_log_module_unused

#define Z_LOG_NOP(...)        \
  do {                        \
    if (0) {                  \
      z_log_nop(__VA_ARGS__); \
    }                         \
  } while (0)

#define LOG_ERR(...)      Z_LOG_NOP(__VA_ARGS__)
#define LOG_WRN(...)      Z_LOG_NOP(__VA_ARGS__)
#define LOG_INF(...)      Z_LOG_NOP(__VA_ARGS__)
#define LOG_DBG(...)      Z_LOG_NOP(__VA_ARGS__)
#define LOG_WRN_ONCE(...) Z_LOG_NOP(__VA_ARGS__)
#define LOG_ERR_ONCE(...) Z_LOG_NOP(__VA_ARGS__)
#define LOG_PRINTK(...)   Z_LOG_NOP(__VA_ARGS__)
#define LOG_RAW(...)      Z_LOG_NOP(__VA_ARGS__)

#define Z_LOG_HEXDUMP_NOP(data, len, str) \
  do {                                    \
    if (0) {                              \
      z_log_hexdump_nop(data, len, str);  \
    }                                     \
  } while (0)

#define LOG_HEXDUMP_ERR(data, len, str) Z_LOG_HEXDUMP_NOP(data, len, str)
#define LOG_HEXDUMP_WRN(data, len, str) Z_LOG_HEXDUMP_NOP(data, len, str)
#define LOG_HEXDUMP_INF(data, len, str) Z_LOG_HEXDUMP_NOP(data, len, str)
#define LOG_HEXDUMP_DBG(data, len, str) Z_LOG_HEXDUMP_NOP(data, len, str)

#define LOG_PANIC() \
  do {              \
  } while (0)
