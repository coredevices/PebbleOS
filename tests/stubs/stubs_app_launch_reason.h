/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "pbl/kernel/compiler.h"

AppLaunchReason PBL_WEAK app_launch_reason(void) {
  return APP_LAUNCH_SYSTEM;
}

ButtonId PBL_WEAK app_launch_button(void) {
  return BUTTON_ID_UP;
}

AppQuickLaunchAction PBL_WEAK app_launch_get_quick_launch_action(void) {
  return APP_QUICK_LAUNCH_ACTION_NONE;
}

uint32_t PBL_WEAK app_launch_get_args(void) {
  return 0;
}
