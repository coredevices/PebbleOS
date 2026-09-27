/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef _NIMBLE_NPL_OS_LOG_H_
#define _NIMBLE_NPL_OS_LOG_H_

#include <stdarg.h>

#include <pbl/logging/logging.h>

/* NimBLE to PebbleOS log level equivalences */
#define NIMBLE_LOG_LEVEL_DEBUG    LOG_LEVEL_DEBUG
#define NIMBLE_LOG_LEVEL_INFO     LOG_LEVEL_INFO
#define NIMBLE_LOG_LEVEL_WARN     LOG_LEVEL_WARNING
#define NIMBLE_LOG_LEVEL_ERROR    LOG_LEVEL_ERROR
#define NIMBLE_LOG_LEVEL_CRITICAL LOG_LEVEL_ALWAYS

/* Host sources cannot declare a PBL log module, so check the NimBLE module level directly */
#define NIMBLE_SHOULD_LOG(level) ((level) <= CONFIG_NIMBLE_LOG_LEVEL)

#define BLE_NPL_LOG_IMPL(lvl)                                                        \
  static inline void _BLE_NPL_LOG_CAT(BLE_NPL_LOG_MODULE, _BLE_NPL_LOG_CAT(_, lvl))( \
      const char *fmt, ...) {                                                        \
    if (NIMBLE_SHOULD_LOG(NIMBLE_LOG_LEVEL_##lvl)) {                                 \
      va_list args;                                                                  \
      va_start(args, fmt);                                                           \
      pbl_log_vargs(NIMBLE_LOG_LEVEL_##lvl, "", 0, fmt, args);                       \
      va_end(args);                                                                  \
    }                                                                                \
  }

#endif /* _NIMBLE_NPL_OS_LOG_H_ */
