/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_install.h"

#include <syscall/syscall.h>

AppInstallResult app_install_count(uint32_t *count) {
  return sys_app_install_count(count);
}

AppInstallResult app_install_get_at_index(uint32_t index, AppInstallInfo *info) {
  return sys_app_install_get_at_index(index, info);
}

AppInstallResult app_launch(const Uuid *uuid) {
  return sys_app_launch(uuid);
}

AppInstallResult watchface_select(const Uuid *uuid) {
  return sys_watchface_select(uuid);
}
