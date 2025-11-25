/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct McuRebootReason {
  union {
    struct {
      bool brown_out_reset:1;
      bool pin_reset:1;
      bool power_on_reset:1;
      bool software_reset:1;
      bool independent_watchdog_reset:1;
      bool window_watchdog_reset:1;
      bool low_power_manager_reset:1;
      uint8_t reserved:1;
    };
    uint8_t reset_mask;
  };
} McuRebootReason;
