/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "shell/normal/quick_launch.h"
#include "pbl/util/attributes.h"

void WEAK quick_launch_remove_app(const Uuid *uuid) {}

AppInstallId WEAK quick_launch_get_app(ButtonId button) {
  return 0;
}

AppInstallId WEAK quick_launch_single_click_get_app(ButtonId button) {
  return 0;
}

AppInstallId WEAK quick_launch_combo_back_up_get_app(void) {
  return 0;
}

AppInstallId WEAK quick_launch_combo_up_down_get_app(void) {
  return 0;
}
