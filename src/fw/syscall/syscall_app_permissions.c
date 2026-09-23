/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

#include "pbl/services/app_permissions/app_permissions.h"

DEFINE_SYSCALL(uint8_t, sys_app_permission_get_state, uint8_t permission) {
  if (permission >= AppPermissionCount) {
    syscall_failed();
  }
#ifdef CONFIG_SERVICE_APP_PERMISSIONS
  return (uint8_t)app_permissions_get_state_for_current_app((AppPermission)permission);
#else
  // Builds without the service (e.g. PRF) never grant anything.
  return (uint8_t)AppPermissionStateDenied;
#endif
}
