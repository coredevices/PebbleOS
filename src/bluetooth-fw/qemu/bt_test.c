/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/bt_test.h>

void bt_driver_test_start(void) {
}

void bt_driver_test_enter_hci_passthrough(void) {
}

void bt_driver_test_handle_hci_passthrough_character(char c, bool *should_context_switch) {
}

bool bt_driver_test_enter_rf_test_mode(void) {
  return true;
}

void bt_driver_test_set_spoof_address(const BTDeviceAddress *addr) {
}

void bt_driver_test_stop(void) {
}

bool bt_driver_test_selftest(void) {
  return true;
}

bool bt_driver_test_mfi_chip_selftest(void) {
  return false;
}

void bt_driver_core_dump(BtleCoreDump type) {
}
