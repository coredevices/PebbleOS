/*
 * Copyright 2025 Core Devices LLC
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

#include "board/board.h"
#include "services/common/battery/battery_curve.h"

uint32_t battery_curve_get_percent_remaining(uint32_t hours) {
  return ((hours * 100) / BOARD_CONFIG_POWER.battery_capacity_hours) +
    BOARD_CONFIG_POWER.low_power_threshold;
}

// TODO: nRF Fuel gauge lib provides TTE estimation
// First we need to evaluate if it's good enough for our use case, then, if
// it is, we need to refactor APIs and battery FSM a bit to use TTE value
// directly (i.e. not from % remaining).
uint32_t battery_curve_get_hours_remaining(uint32_t percent_remaining) {
  if (percent_remaining <= BOARD_CONFIG_POWER.low_power_threshold) {
    return 0;
  }
  percent_remaining -= BOARD_CONFIG_POWER.low_power_threshold;
  return ((BOARD_CONFIG_POWER.battery_capacity_hours * percent_remaining) / 100);
}

// TODO: use nRF Fuel gauge SoC calculation
// Only used in PRF MFG, not actively used now so can probably be removed
uint32_t battery_curve_lookup_percent_by_voltage(uint32_t battery_mv, bool is_charging) {
  return 100U;
}

#if CAPABILITY_HAS_LED
// TODO: Probably can go away
void battery_curve_set_compensation(BatteryCurveVoltageCompensationKey key, int mv) {
}
#endif

// Stubs for tests (need fixing/test adjustments)
int32_t battery_curve_lookup_percent_with_scaling_factor(
    int battery_mv, bool is_charging, uint32_t scaling_factor) {
  return 0U;
}

uint32_t battery_curve_lookup_voltage_by_percent(uint32_t percent, bool is_charging) {
  return 0U;
}