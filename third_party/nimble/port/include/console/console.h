/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <nimble/nimble_npl_os_log.h>

#define console_printf(_fmt, ...)                          \
  do {                                                     \
    if (NIMBLE_SHOULD_LOG(LOG_LEVEL_INFO)) {               \
      pbl_log(LOG_LEVEL_INFO, "", 0, _fmt, ##__VA_ARGS__); \
    }                                                      \
  } while (0)

#endif /* __CONSOLE_H__ */
