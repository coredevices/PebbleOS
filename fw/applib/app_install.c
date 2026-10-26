/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_install.h"

#include <syscall/syscall.h>

uint32_t app_install_count(void) {
  return sys_app_install_count();
}

bool app_install_get_at_index(uint32_t index, AppInstallInfo *info) {
  return sys_app_install_get_at_index(index, info);
}

bool app_launch(const Uuid *uuid) {
  return sys_app_launch(uuid);
}

bool watchface_select(const Uuid *uuid) {
  return sys_watchface_select(uuid);
}
