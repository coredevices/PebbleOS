/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/battery.h"
#include "fake_battery.h"
#include "kernel/events.h"

static int s_millivolts = 0;
static bool s_usb_connected = false;
static bool s_charging = false;

void fake_battery_init(int millivolts, bool usb_connected, bool charging) {
  s_millivolts = millivolts;
  s_usb_connected = usb_connected;
  s_charging = charging;
}

void fake_battery_set_millivolts(int millivolts) {
  s_millivolts = millivolts;
}

void fake_battery_set_connected(bool usb_connected) {
  s_usb_connected = usb_connected;

  // Trigger a connection event!
  PebbleEvent event = {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .battery_connection = {
      .is_connected = usb_connected,
    }
  };

  event_put(&event);
}

void fake_battery_set_charging(bool charging) {
  s_charging = charging;
}

int battery_get_millivolts(void) {
  return s_millivolts;
}

bool battery_is_usb_connected(void) {
  return s_usb_connected;
}

bool battery_charge_controller_thinks_we_are_charging(void) {
  return s_charging;
}

int battery_get_constants(BatteryConstants *constants) {
  if (!constants) {
    return -1;
  }
  constants->v_mv = s_millivolts;
  constants->i_ua = 100;
  constants->t_mc = 25000;
  return 0;
}

int battery_charge_status_get(BatteryChargeStatus *status) {
  if (!status) {
    return -1;
  }
  *status = s_charging ? BatteryChargeStatusCC : BatteryChargeStatusUnknown;
  return 0;
}

bool battery_is_usb_connected_impl(void) {
  return s_usb_connected;
}
