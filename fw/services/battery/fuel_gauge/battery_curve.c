/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/battery/battery_curve.h>

uint32_t battery_curve_get_percent_remaining(uint32_t hours) {
  return ((hours * 100) / CONFIG_SERVICE_BATTERY_CAPACITY_HOURS) +
         CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT;
}

uint32_t battery_curve_get_hours_remaining(uint32_t percent_remaining) {
  if (percent_remaining <= CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT) {
    return 0;
  }
  percent_remaining -= CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT;
  return ((CONFIG_SERVICE_BATTERY_CAPACITY_HOURS * percent_remaining) / 100);
}
