/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/bt_test.h>

void bt_driver_test_start(void) {}

void bt_driver_test_enter_hci_passthrough(void) {}

void bt_driver_test_handle_hci_passthrough_character(char c, bool *should_context_switch) {}

bool bt_driver_test_enter_rf_test_mode(void) { return true; }

void bt_driver_test_set_spoof_address(const BTDeviceAddress *addr) {}

void bt_driver_test_stop(void) {}

bool bt_driver_test_selftest(void) { return true; }

bool bt_driver_test_mfi_chip_selftest(void) { return false; }

void bt_driver_core_dump(BtleCoreDump type) {}
