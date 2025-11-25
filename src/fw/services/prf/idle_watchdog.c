/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "idle_watchdog.h"

#include "applib/event_service_client.h"
#include "comm/ble/gap_le_connection.h"
#include "services/common/battery/battery_monitor.h"
#include "services/common/regular_timer.h"
#include "services/common/system_task.h"
#include "system/reboot_reason.h"
#include "kernel/util/standby.h"

#include <bluetooth/classic_connect.h>

#define PRF_IDLE_TIMEOUT_MINUTES 10
static RegularTimerInfo s_is_idle_timer;


static void prv_handle_watchdog_timeout_cb(void *not_used) {
  GAPLEConnection *le_connection = gap_le_connection_any();

  if (le_connection || bt_driver_classic_is_connected()) {
    // We are still connected, don't shut down
    return;
  }

  BatteryChargeState current_state = battery_get_charge_state();
  if (current_state.is_plugged) {
    // We are plugged in, don't shut down
    return;
  }

  enter_standby(RebootReasonCode_PrfIdle);
}

static void prv_handle_watchdog_timeout(void *not_used) {
  system_task_add_callback(prv_handle_watchdog_timeout_cb, NULL);
}

static void prv_start_watchdog(void) {
  s_is_idle_timer = (const RegularTimerInfo) {
    .cb = prv_handle_watchdog_timeout,
  };

  regular_timer_add_multiminute_callback(&s_is_idle_timer,
                                         PRF_IDLE_TIMEOUT_MINUTES);
}

void prv_watchdog_feed(PebbleEvent *e, void *context) {
  if (regular_timer_is_scheduled(&s_is_idle_timer)) {
    prv_start_watchdog();
  }
}


void prf_idle_watchdog_start(void) {
  // Possible scenario: connect -> 9.9 minutes elapse -> disconnect
  // Feeding the watchdog on bt events ensures we don't shutdown after being
  // idle for only 0.1 minutes
  static EventServiceInfo bt_event_info;
  bt_event_info = (EventServiceInfo) {
    .type = PEBBLE_BT_CONNECTION_EVENT,
    .handler = prv_watchdog_feed,
  };
  event_service_client_subscribe(&bt_event_info);

  // Possible scenario: plug in watch to charge -> 9.9 minutes elapse -> remove watch from charger
  // Feeding the watchdog on usb events ensures we don't shutdown as the watch is about to be used
  static EventServiceInfo battery_event_info;
  battery_event_info = (EventServiceInfo) {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .handler = prv_watchdog_feed,
  };
  event_service_client_subscribe(&battery_event_info);

  // The watch is clearly being used if a button was pressed
  static EventServiceInfo button_event_info;
  button_event_info = (EventServiceInfo) {
    .type = PEBBLE_BUTTON_DOWN_EVENT,
    .handler = prv_watchdog_feed,
  };
  event_service_client_subscribe(&button_event_info);

  prv_start_watchdog();
}

void prf_idle_watchdog_stop(void) {
  if (regular_timer_is_scheduled(&s_is_idle_timer)) {
    regular_timer_remove_callback(&s_is_idle_timer);
  }
}
