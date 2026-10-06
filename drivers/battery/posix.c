/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/battery.h>

// A full battery, not charging.

void battery_init(void) {
}

int battery_get_millivolts(void) {
  return 4200;
}

int battery_get_constants(BatteryConstants *constants) {
  constants->v_mv = 4200;
  constants->i_ua = 100;
  constants->t_mc = 25000;
  return 0;
}

int battery_charge_status_get(BatteryChargeStatus *status) {
  *status = BatteryChargeStatusUnknown;
  return 0;
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  return false;
}

bool battery_is_usb_connected_impl(void) {
  return false;
}

void battery_set_charge_enable(bool charging_enabled) {
}

void battery_set_fast_charge(bool fast_charge_enabled) {
}
