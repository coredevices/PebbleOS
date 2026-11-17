/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/activity/health_permission.h>

PBL_WEAK bool health_permission_allowed(PebbleTask task) {
  return true;
}

PBL_WEAK void health_permission_handle_changed(const Uuid *uuid) {
}

PBL_WEAK bool sys_activity_health_allowed(void) {
  return true;
}
