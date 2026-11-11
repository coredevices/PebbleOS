/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/battery.h>
#include <pbl/drivers/battery/sim.h>
#include <pbl/services/battery/battery_curve.h>
#include <pbl/util/math.h>

static uint8_t s_percent = 100;
static bool s_charger_connected;
static int s_mv;

void pbl_battery_sim_set(uint8_t percent, bool charger_connected) {
  bool changed = (charger_connected != s_charger_connected);

  s_percent = MIN(percent, 100);
  s_charger_connected = charger_connected;
  s_mv = battery_curve_lookup_voltage_by_percent(s_percent, charger_connected);

  if (changed) {
    battery_connection_changed();
  }
}

uint8_t pbl_battery_sim_get_percent(void) {
  return s_percent;
}

void battery_init(void) {
  s_mv = battery_curve_lookup_voltage_by_percent(s_percent, s_charger_connected);
}

int battery_get_millivolts(void) {
  return s_mv;
}

int battery_get_constants(BatteryConstants *constants) {
  constants->v_mv = s_mv;
  constants->i_ua = 100;
  constants->t_mc = 25000;
  return 0;
}

int battery_charge_status_get(BatteryChargeStatus *status) {
  *status = BatteryChargeStatusUnknown;
  return 0;
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  return s_charger_connected && s_percent < 100;
}

bool battery_is_usb_connected_impl(void) {
  return s_charger_connected;
}

void battery_set_charge_enable(bool charging_enabled) {
}

void battery_set_fast_charge(bool fast_charge_enabled) {
}
