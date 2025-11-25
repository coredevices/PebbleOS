/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session_remote_version.h"
#include "util/attributes.h"

void WEAK bt_persistent_storage_get_cached_system_capabilities(
    PebbleProtocolCapabilities *capabilities_out) {
  if (capabilities_out) {
    capabilities_out->flags = 0;
  }
}
