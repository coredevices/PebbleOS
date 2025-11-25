/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/bt_lock.h"
#include "drivers/qemu/qemu_serial.h"
#include "drivers/qemu/qemu_settings.h"
#include "kernel/event_loop.h"
#include "pebble_errors.h"
#include "system/logging.h"

#include <bluetooth/init.h>
#include <bluetooth/qemu_transport.h>

#include <stdlib.h>

// ----------------------------------------------------------------------------------------
static void prv_set_connected_cb(void *context) {
  qemu_transport_set_connected(true);
}

// ----------------------------------------------------------------------------------------
void bt_driver_init(void) {
  // We need the QEMU serial driver
  qemu_serial_init();
  bt_lock_init();
}

bool bt_driver_start(BTDriverConfig *config) {
  // The first time the stack starts, use the QemuSetting_DefaultConnected setting.
  // Subsequent times, always auto-connect when the stack starts.
  static bool s_should_auto_connect = false;

  // Have KernelMain set us to connected once the event loop starts up, this gives enough time
  // for the launcher to init its app message callbacks
  if (s_should_auto_connect || qemu_setting_get(QemuSetting_DefaultConnected)) {
    launcher_task_add_callback(prv_set_connected_cb, NULL);
    s_should_auto_connect = true;
  }
  return true;
}

void bt_driver_stop(void) {
  qemu_transport_set_connected(false);
}

void bt_driver_power_down_controller_on_boot(void) {
  // no-op
}
