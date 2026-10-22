/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bas.h>
#include <pbl/services/bluetooth/battery_level.h>

#include <applib/event_service_client.h>
#include <kernel/event_loop.h>
#include <kernel/pebble_tasks.h>
#include <syscall/syscall.h>

static EventServiceInfo s_battery_evt;

static void prv_execute_on_kernel_main(CallbackEventCallback cb) {
  if (pebble_task_get_current() != PebbleTask_KernelMain) {
    launcher_task_add_callback(cb, nullptr);
  } else {
    cb(nullptr);
  }
}

static void prv_battery_state_event_handler(PebbleEvent *e, void *context) {
  const PebbleBatteryStateChangeEvent *const battery_state_event = &e->battery_state;

  pbl_bt_bas_handle_update(battery_state_event->new_state.pct);
}

static void prv_start_kernel_main(void *unused) {
  BatteryChargeState battery_state;

  battery_state = sys_battery_get_charge_state();
  pbl_bt_bas_handle_update(battery_state.charge_percent);

  s_battery_evt = (EventServiceInfo){
    .type = PEBBLE_BATTERY_STATE_CHANGE_EVENT,
    .handler = prv_battery_state_event_handler,
  };

  event_service_client_subscribe(&s_battery_evt);
}

static void prv_stop_kernel_main(void *unused) {
  event_service_client_unsubscribe(&s_battery_evt);
}

void bt_battery_level_start(void) {
  prv_execute_on_kernel_main(prv_start_kernel_main);
}

void bt_battery_level_stop(void) {
  prv_execute_on_kernel_main(prv_stop_kernel_main);
}
