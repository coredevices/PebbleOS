/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/comm_session/session_remote_version.h>

void PBL_WEAK
bt_persistent_storage_get_cached_system_capabilities(PebbleProtocolCapabilities *capabilities_out) {
  if (capabilities_out) {
    capabilities_out->flags = 0;
  }
}
