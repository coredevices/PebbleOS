/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/bt_lock.h"
#include "kernel/event_loop.h"
#include <pbl/logging/logging.h>

#include <pbl/bluetooth/init.h>

#include <stdlib.h>

// ----------------------------------------------------------------------------------------
void pbl_bt_init(void) {
  bt_lock_init();
}

bool pbl_bt_start(struct pbl_bt_config *config) {
  return true;
}

void pbl_bt_stop(void) {
}

void pbl_bt_power_down_controller_on_boot(void) {
  // no-op
}
