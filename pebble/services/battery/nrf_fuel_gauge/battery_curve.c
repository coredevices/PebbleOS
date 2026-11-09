/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/battery/battery_curve.h>

uint32_t battery_curve_get_percent_remaining(uint32_t hours) {
  return ((hours * 100) / CONFIG_SERVICE_BATTERY_CAPACITY_HOURS) +
         CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT;
}

// TODO: nRF Fuel gauge lib provides TTE estimation
// First we need to evaluate if it's good enough for our use case, then, if
// it is, we need to refactor APIs and battery FSM a bit to use TTE value
// directly (i.e. not from % remaining).
uint32_t battery_curve_get_hours_remaining(uint32_t percent_remaining) {
  if (percent_remaining <= CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT) {
    return 0;
  }
  percent_remaining -= CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT;
  return ((CONFIG_SERVICE_BATTERY_CAPACITY_HOURS * percent_remaining) / 100);
}

// Stubs for tests (need fixing/test adjustments)
int32_t battery_curve_lookup_percent_with_scaling_factor(int battery_mv, bool is_charging,
                                                         uint32_t scaling_factor) {
  return 0U;
}

uint32_t battery_curve_lookup_voltage_by_percent(uint32_t percent, bool is_charging) {
  return 0U;
}