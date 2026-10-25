/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>

#include <applib/app_launch_button.h>
#include <applib/app_launch_reason.h>

typedef struct LaunchConfigCommon {
  AppLaunchReason reason;
  ButtonId button;
  // Separate from app-specific args (e.g. TimelineArgs).
  AppQuickLaunchAction quick_launch_action;
  const void *args;
  const CompositorTransition *transition;
} LaunchConfigCommon;
