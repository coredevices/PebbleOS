/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/bluetooth/bt_settings.h>

void PBL_WEAK bt_settings_get_capabilities(struct pbl_pp_capabilities *capabilities_out) {
  capabilities_out->flags = 0;
}

void PBL_WEAK bt_settings_set_capabilities(const struct pbl_pp_capabilities *capabilities) {
}

void PBL_WEAK bt_settings_set_unfaithful(bool is_unfaithful) {
}
