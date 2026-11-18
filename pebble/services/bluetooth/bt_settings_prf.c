/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/bluetooth/bt_settings.h>

void bt_settings_init(void) {
}

bool bt_settings_get_airplane_mode(void) {
  return false;
}

void bt_settings_set_airplane_mode(bool enable) {
}

bool bt_settings_is_unfaithful(void) {
  return true;
}

void bt_settings_set_unfaithful(bool is_unfaithful) {
}

void bt_settings_get_capabilities(struct pbl_pp_capabilities *capabilities_out) {
  *capabilities_out = (struct pbl_pp_capabilities){};
}

void bt_settings_set_capabilities(const struct pbl_pp_capabilities *capabilities) {
}
