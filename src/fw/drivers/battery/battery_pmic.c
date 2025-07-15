/*
 * Copyright 2024 Google LLC
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

#include "drivers/battery.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "system/logging.h"
#include "system/passert.h"

void battery_init(void) {
  // pmic_init() needs to happen, but I think it will happen elsewhere first
  // It may be okay to just init the pmic here again
  return;
}

int battery_get_millivolts(void) {
  if (!pmic_enable_battery_measure()) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to enable battery measure. "
            "Battery voltage reading will be bogus.");
  }
  ADCVoltageMonitorReading info = battery_read_voltage_monitor();
  pmic_disable_battery_measure();

  PBL_ASSERTN(BOARD_CONFIG_POWER.battery_vmon_scale.denominator);
  return battery_convert_reading_to_millivolts(info,
                                               BOARD_CONFIG_POWER.battery_vmon_scale.numerator,
                                               BOARD_CONFIG_POWER.battery_vmon_scale.denominator);
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  return pmic_is_charging();
}

bool battery_is_usb_connected_impl(void) {
  return pmic_is_usb_connected();
}

void battery_set_charge_enable(bool charging_enabled) {
  pmic_set_charger_state(charging_enabled);
}

// TODO
// This is my understanding from Figure 9 of the datasheet:
// Charger off -> Pre charge -> Fast Charge (constant current) ->
// Fast Charge (constant voltage) -> Maintain Charge -> Maintain Charge Done
//
// The Pre Charge and Charge Termination currents are programmed via I2C
// The Fast Charge current is determined by Rset
//
// There doesn't seem to be a way to change the current in constant current mode
void battery_set_fast_charge(bool fast_charge_enabled) {
  return;
}
