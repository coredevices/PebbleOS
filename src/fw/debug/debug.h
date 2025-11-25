/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/mcu_reboot_reason.h"
#include <stdint.h>

// TODO: Eventually move debug logging back to hashed logging
// Currently broken out to directly log strings without hashing
#ifdef PBL_LOG_ENABLED
  #define DEBUG_LOG(level, fmt, ...) \
      pbl_log(level, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
  #define DEBUG_LOG(level, fmt, ...)
#endif

void debug_init(McuRebootReason reason);

void debug_print_last_launched_app(void);

