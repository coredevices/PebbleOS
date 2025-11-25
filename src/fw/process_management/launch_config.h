/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "services/common/compositor/compositor.h"

typedef struct LaunchConfigCommon {
  AppLaunchReason reason;
  ButtonId button;
  const void *args;
  const CompositorTransition *transition;
} LaunchConfigCommon;
