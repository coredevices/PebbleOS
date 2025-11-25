/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/battery.h"
#include "drivers/qemu/qemu_serial.h"
#include "drivers/qemu/qemu_settings.h"

#include "system/passert.h"
#include "services/common/battery/battery_state.h"
#include "services/common/battery/battery_curve.h"
#include "system/logging.h"

#include "util/math.h"
#include "util/net.h"

static uint16_t s_battery_mv = 4000;
static bool s_usb_connected;
static uint8_t s_percent = 100;

void battery_init(void) {
  s_usb_connected = qemu_setting_get(QemuSetting_DefaultPluggedIn);
}

// TODO: update whoever uses this function
int battery_get_millivolts(void) {
  return s_battery_mv;
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  return s_usb_connected && (s_percent < 100);
}

bool battery_is_usb_connected_impl(void) {
  return s_usb_connected;
}

void battery_set_charge_enable(bool charging_enabled) {
  s_usb_connected = false;
}

void battery_set_fast_charge(bool fast_charge_enabled) {
}


void qemu_battery_msg_callack(const uint8_t *data, uint32_t len) {
  QemuProtocolBatteryHeader *hdr = (QemuProtocolBatteryHeader *)data;
  if (len != sizeof(*hdr)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid packet length");
    return;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Got battery msg: pct: %d, charger_connected:%d",
        hdr->battery_pct, hdr->charger_connected);

  s_percent = MIN(100, hdr->battery_pct);
  s_usb_connected = hdr->charger_connected;
  s_battery_mv = battery_curve_lookup_voltage_by_percent(s_percent, hdr->charger_connected);

  // Reset the time averaging so these new values take effect immediately
  battery_state_reset_filter();

  // Force a state machine update
  battery_state_handle_connection_event(s_usb_connected);
}


