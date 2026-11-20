/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/battery.h>
#include <pbl/drivers/charger.h>

static bool s_charging_forced_disable = false;
static battery_connection_handler_t s_connection_handler;

static void prv_online_changed(const struct pbl_charger *charger) {
  battery_connection_changed();
}

static const struct pbl_charger_listener s_listener = {
  .online_changed = prv_online_changed,
};

void battery_init(void) {
}

void battery_set_connection_handler(battery_connection_handler_t handler) {
  s_connection_handler = handler;
  pbl_charger_set_listener(CHARGER, (handler != nullptr) ? &s_listener : nullptr);
}

void battery_connection_changed(void) {
  battery_connection_handler_t handler = s_connection_handler;

  if (handler != nullptr) {
    handler(battery_is_usb_connected());
  }
}

int battery_get_millivolts(void) {
  struct pbl_charger_measurements meas;

  if (pbl_charger_get_measurements(CHARGER, &meas) != 0) {
    return 0;
  }

  return meas.voltage_mv;
}

int battery_get_constants(BatteryConstants *constants) {
  struct pbl_charger_measurements meas;
  int ret;

  ret = pbl_charger_get_measurements(CHARGER, &meas);
  if (ret != 0) {
    return ret;
  }

  constants->v_mv = meas.voltage_mv;
  constants->i_ua = meas.current_ua;
  constants->t_mc = meas.temp_mc;

  return 0;
}

int battery_charge_status_get(BatteryChargeStatus *status) {
  enum pbl_charger_status chg_status;
  int ret;

  ret = pbl_charger_get_status(CHARGER, &chg_status);
  if (ret != 0) {
    return ret;
  }

  switch (chg_status) {
    case PBL_CHARGER_STATUS_FULL:
      *status = BatteryChargeStatusComplete;
      break;
    case PBL_CHARGER_STATUS_TRICKLE:
      *status = BatteryChargeStatusTrickle;
      break;
    case PBL_CHARGER_STATUS_CONSTANT_CURRENT:
      *status = BatteryChargeStatusCC;
      break;
    case PBL_CHARGER_STATUS_CONSTANT_VOLTAGE:
      *status = BatteryChargeStatusCV;
      break;
    default:
      *status = BatteryChargeStatusUnknown;
      break;
  }

  return 0;
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  bool charging = false;

  (void)pbl_charger_is_charging(CHARGER, &charging);

  return charging;
}

bool battery_is_usb_connected_impl(void) {
  bool online = false;

  (void)pbl_charger_is_online(CHARGER, &online);

  return online;
}

void battery_set_charge_enable(bool charging_enabled) {
  (void)pbl_charger_enable(CHARGER, charging_enabled);
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

#ifdef CONFIG_CHARGER_HIGH_LOAD
int battery_high_load_request(void) {
  return pbl_charger_high_load_request(CHARGER);
}

int battery_high_load_release(void) {
  return pbl_charger_high_load_release(CHARGER);
}
#endif
