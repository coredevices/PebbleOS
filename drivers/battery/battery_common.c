/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/battery.h>
#include <pbl/drivers/gpio.h>

static bool s_charging_forced_disable = false;
static battery_connection_handler_t s_connection_handler;

void battery_set_connection_handler(battery_connection_handler_t handler) {
  s_connection_handler = handler;
}

void battery_connection_changed(void) {
  battery_connection_handler_t handler = s_connection_handler;

  if (handler != nullptr) {
    handler(battery_is_usb_connected());
  }
}

bool battery_charge_controller_thinks_we_are_charging(void) {
  if (s_charging_forced_disable) {
    return false;
  }

  return battery_charge_controller_thinks_we_are_charging_impl();
}

bool battery_is_usb_connected(void) {
  if (s_charging_forced_disable) {
    return false;
  }

  return battery_is_usb_connected_impl();
}

void battery_force_charge_enable(bool charging_enabled) {
  s_charging_forced_disable = !charging_enabled;

  battery_set_charge_enable(charging_enabled);
}
