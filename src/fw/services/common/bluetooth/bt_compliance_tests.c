/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/console_internal.h"
#include "console/prompt.h"

#include "services/common/bluetooth/bluetooth_ctl.h"
#include "services/common/bluetooth/bt_compliance_tests.h"

#include "kernel/util/stop.h"

#include <bluetooth/bt_test.h>

static bool s_test_mode_enabled = false;

void bt_test_start(void) {
  if (s_test_mode_enabled) {
    prompt_send_response("Invalid operation: Run 'bt test stop' first");
    return;
  }
  s_test_mode_enabled = true;
  bt_ctl_set_override_mode(BtCtlModeOverrideStop);
  stop_mode_disable(InhibitorBluetooth);

  bt_driver_test_start();
}

bool bt_test_bt_sig_rf_test_mode(void) {
  return bt_driver_test_enter_rf_test_mode();
}

void bt_test_enter_hci_passthrough(void) {
  // redirect all communications to the BT module
  serial_console_set_state(SERIAL_CONSOLE_STATE_HCI_PASSTHROUGH);

  bt_driver_test_enter_hci_passthrough();
}

void bt_test_stop(void) {
  if (!s_test_mode_enabled) {
    prompt_send_response("Invalid operation: Run 'bt test start' first");
    return;
  }

  bt_driver_test_stop();
  stop_mode_enable(InhibitorBluetooth);

  // Bring the normal BT stack back up - airplane mode makes this simple
  bt_ctl_set_override_mode(BtCtlModeOverrideNone);
  s_test_mode_enabled = false;
}
