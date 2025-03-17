/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NIMBLE_NPL_OS_LOG_H_
#define _NIMBLE_NPL_OS_LOG_H_

#include <stdarg.h>

#include "system/logging.h"

/* NimBLE to PebbleOS log level equivalences */
#define NIMBLE_LOG_LEVEL_DEBUG LOG_LEVEL_DEBUG
#define NIMBLE_LOG_LEVEL_INFO LOG_LEVEL_INFO
#define NIMBLE_LOG_LEVEL_WARN LOG_LEVEL_WARNING
#define NIMBLE_LOG_LEVEL_ERROR LOG_LEVEL_ERROR
#define NIMBLE_LOG_LEVEL_CRITICAL LOG_LEVEL_ALWAYS

#define BLE_NPL_LOG_IMPL(lvl)                                                        \
  static inline void _BLE_NPL_LOG_CAT(BLE_NPL_LOG_MODULE, _BLE_NPL_LOG_CAT(_, lvl))( \
      const char *fmt, ...) {                                                        \
    if (LOG_DOMAIN_BT) {                                                             \
      va_list args;                                                                  \
      va_start(args, fmt);                                                           \
      pbl_log_vargs(NIMBLE_LOG_LEVEL_##lvl, __FILE__, __LINE__, fmt, args);          \
      va_end(args);                                                                  \
    }                                                                                \
  }

#endif /* _NIMBLE_NPL_OS_LOG_H_ */
