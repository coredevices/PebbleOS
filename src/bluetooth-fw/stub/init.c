/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/bt_lock.h"
#include "kernel/event_loop.h"
#include "pebble_errors.h"
#include "system/logging.h"

#include <bluetooth/init.h>

#include <stdlib.h>

// ----------------------------------------------------------------------------------------
void bt_driver_init(void) {
  bt_lock_init();
}

bool bt_driver_start(BTDriverConfig *config) {
  return true;
}

void bt_driver_stop(void) {
}

void bt_driver_power_down_controller_on_boot(void) {
  // no-op
}
