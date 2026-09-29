/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "pbl/services/battery/battery_curve.h"

uint32_t battery_curve_get_percent_remaining(uint32_t hours) {
  return ((hours * 100) / BOARD_CONFIG_POWER.battery_capacity_hours) +
         BOARD_CONFIG_POWER.low_power_threshold;
}

uint32_t battery_curve_get_hours_remaining(uint32_t percent_remaining) {
  if (percent_remaining <= BOARD_CONFIG_POWER.low_power_threshold) {
    return 0;
  }
  percent_remaining -= BOARD_CONFIG_POWER.low_power_threshold;
  return ((BOARD_CONFIG_POWER.battery_capacity_hours * percent_remaining) / 100);
}
