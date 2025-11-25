/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "shell/shell_event_loop.h"

#include <bluetooth/reconnect.h>

#include "popups/bluetooth_pairing_ui.h"

#include "services/common/analytics/analytics.h"
#include "services/prf/idle_watchdog.h"

static bool s_paused_reconnect_because_repairing = false;

static void prv_pause_reconnect_if_needed(void) {
  // See https://pebbletechnology.atlassian.net/browse/PBL-13231
  // iOS has a really annoying bug that causes it to automatically start pairing if it has no
  // pairing yet, but it does not present the confirmation UI, unless the user is in Bluetooth
  // Settings, OR, if the user has tapped the device from the EAAccessory device picker.
  // However, chances are neither are the case... When this happens, a pairing UI will show up
  // on Pebble, but nothing will show up on the iOS end.
  // This situation will occur if the user got into PRF and forgets the pairing in iOS (or the
  // other way around. Unfortunately, when PRF initiates the reconnection, there is no way to know
  // whether iOS still has the pairing (the user might have removed it). When a pairing event is
  // received, Pebble can also not know whether the confirmation UI is showing on iOS. However, it
  // probably means the other side forgot the previous pairing, so make Pebble stop
  // auto-reconnecting until reboot, so that the number of times the bug is hit is at least limited
  // to one time... :((((
  if (!s_paused_reconnect_because_repairing) {
    bt_driver_reconnect_pause();
    s_paused_reconnect_because_repairing = true;
  }
}

static void prv_resume_reconnect_if_needed(void) {
if (s_paused_reconnect_because_repairing) {
    bt_driver_reconnect_resume();
    s_paused_reconnect_because_repairing = false;
  }
}

void shell_event_loop_init(void) {
#ifndef MANUFACTURING_FW
  prf_idle_watchdog_start();
#endif
}

void shell_event_loop_handle_event(PebbleEvent *e) {
  switch(e->type) {
  case PEBBLE_BT_PAIRING_EVENT:
    if (e->bluetooth.pair.type == PebbleBluetoothPairEventTypePairingComplete) {
      prv_resume_reconnect_if_needed();
    } else {
      prv_pause_reconnect_if_needed();
    }
    bluetooth_pairing_ui_handle_event(&e->bluetooth.pair);
    return;

  default:
    return;
  }
}

