/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/bluetooth/bt_settings.h>

PBL_WEAK void bt_settings_get_capabilities(PebbleProtocolCapabilities *capabilities_out) {
  capabilities_out->flags = 0;
}
