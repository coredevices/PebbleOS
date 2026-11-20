/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/charger.h>
#include <pbl/services/battery/battery_charger.h>

#include <kernel/events.h>

static bool s_charging_forced_disable;

static void prv_online_changed(const struct pbl_charger *charger) {
  PebbleEvent event = {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .battery_connection = {
      .is_connected = battery_is_usb_connected(),
    },
  };

  event_put(&event);
}

static const struct pbl_charger_listener s_listener = {
  .online_changed = prv_online_changed,
};

void battery_charger_init(void) {
  pbl_charger_set_listener(CHARGER, &s_listener);
}

bool battery_is_usb_connected(void) {
  bool online = false;

  if (s_charging_forced_disable) {
    return false;
  }

  (void)pbl_charger_is_online(CHARGER, &online);

  return online;
}

bool battery_charge_controller_thinks_we_are_charging(void) {
  bool charging = false;

  if (s_charging_forced_disable) {
    return false;
  }

  (void)pbl_charger_is_charging(CHARGER, &charging);

  return charging;
}

void battery_force_charge_enable(bool is_charging) {
  s_charging_forced_disable = !is_charging;

  (void)pbl_charger_enable(CHARGER, is_charging);
}
