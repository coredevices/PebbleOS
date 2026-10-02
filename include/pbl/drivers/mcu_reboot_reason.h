/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_mcu_reboot_reason MCU reset cause
 * @ingroup drivers
 * @brief Hardware reset cause reported by the microcontroller.
 *
 * Filled by watchdog_clear_reset_flag(). Causes the hardware cannot report are left clear.
 * @{
 */

/** @brief Reset cause flags. */
typedef struct McuRebootReason {
  union {
    struct {
      /** Brown-out reset. */
      bool brown_out_reset : 1;
      /** Reset pin. */
      bool pin_reset : 1;
      /** Power-on (cold boot) reset. */
      bool power_on_reset : 1;
      /** Software requested reset. */
      bool software_reset : 1;
      /** Independent watchdog reset. */
      bool independent_watchdog_reset : 1;
      /** Window watchdog reset. */
      bool window_watchdog_reset : 1;
      /** Low-power manager reset. */
      bool low_power_manager_reset : 1;
      /** Reserved. */
      uint8_t reserved : 1;
    };
    /** All flags as a bit mask. */
    uint8_t reset_mask;
  };
} McuRebootReason;

/** @} */
