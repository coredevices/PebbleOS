/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/event_service_client.h"
#include "bluetooth/bas.h"
#include "kernel/event_loop.h"
#include "kernel/pebble_tasks.h"
#include "syscall/syscall.h"

static EventServiceInfo s_bas_evt;

static void prv_execute_on_kernel_main(CallbackEventCallback cb) {
  if (pebble_task_get_current() != PebbleTask_KernelMain) {
    launcher_task_add_callback(cb, NULL);
  } else {
    cb(NULL);
  }
}

static void prv_ble_bas_handle_event(PebbleEvent *e, void *context) {
  const PebbleBatteryStateChangeEvent *const battery_state_event = &e->battery_state;

  bt_driver_bas_handle_update(battery_state_event->new_state.pct);
}

static void prv_start_ble_bas_kernel_main(void *unused) {
  BatteryChargeState battery_state;

  battery_state = sys_battery_get_charge_state();
  bt_driver_bas_handle_update(battery_state.charge_percent);

  s_bas_evt = (EventServiceInfo) {
    .type = PEBBLE_BATTERY_STATE_CHANGE_EVENT,
    .handler = prv_ble_bas_handle_event,
  };

  event_service_client_subscribe(&s_bas_evt);
}

static void prv_stop_ble_bas_kernel_main(void *unused) {
  event_service_client_unsubscribe(&s_bas_evt);
}

void ble_bas_init(void) {
  prv_execute_on_kernel_main(prv_start_ble_bas_kernel_main);
}

void ble_bas_deinit(void) {
  prv_execute_on_kernel_main(prv_stop_ble_bas_kernel_main);
}
