/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>

#include <pbl/kernel/work.h>
#include <pbl/services/idle_watchdog.h>

#include <applib/event_service_client.h>
#include <comm/ble/gap_le_connection.h>
#include <kernel/util/standby.h>
#include <system/reboot_reason.h>

#define PRF_IDLE_TIMEOUT_MINUTES 10
static struct pbl_wallclock_work s_is_idle_timer;
static EventServiceInfo s_bt_event_info;
static EventServiceInfo s_battery_event_info;
static EventServiceInfo s_button_event_info;
static atomic_bool s_running;

static void prv_handle_watchdog_timeout_cb(void *not_used) {
  GAPLEConnection *le_connection = gap_le_connection_any();

  if (le_connection) {
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

static void prv_handle_watchdog_timeout(struct pbl_work *work) {
  pbl_sys_work_call(prv_handle_watchdog_timeout_cb, nullptr);
}

static void prv_watchdog_feed(PebbleEvent *e, void *context) {
  if (atomic_load_explicit(&s_running, memory_order_relaxed)) {
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_is_idle_timer, 60, PRF_IDLE_TIMEOUT_MINUTES);
  }
}

void prf_idle_watchdog_init(void) {
  pbl_wallclock_work_init(&s_is_idle_timer, prv_handle_watchdog_timeout);

  s_bt_event_info = (EventServiceInfo){
    .type = PBL_BT_PEBBLE_CONNECTION_EVENT,
    .handler = prv_watchdog_feed,
  };
  event_service_client_subscribe(&s_bt_event_info);

  s_battery_event_info = (EventServiceInfo){
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .handler = prv_watchdog_feed,
  };
  event_service_client_subscribe(&s_battery_event_info);

  s_button_event_info = (EventServiceInfo){
    .type = PEBBLE_BUTTON_DOWN_EVENT,
    .handler = prv_watchdog_feed,
  };
  event_service_client_subscribe(&s_button_event_info);
}

void prf_idle_watchdog_start(void) {
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_is_idle_timer, 60, PRF_IDLE_TIMEOUT_MINUTES);
  atomic_store_explicit(&s_running, true, memory_order_relaxed);
}

void prf_idle_watchdog_stop(void) {
  atomic_store_explicit(&s_running, false, memory_order_relaxed);
  pbl_wallclock_work_stop(&s_is_idle_timer);
}
