/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_install_types.h"
#include "process_management/launch_config.h"
#include "process_management/pebble_process_md.h"
#include "services/common/compositor/compositor.h"
#include "services/normal/wakeup.h"

#include <stdbool.h>

typedef struct AppFetchUIArgs {
  LaunchConfigCommon common;
  WakeupInfo wakeup_info;
  AppInstallId app_id;
  bool forcefully; //! whether to launch forcefully or not
} AppFetchUIArgs;

//! Used to launch the app_fetch_ui application
const PebbleProcessMd *app_fetch_ui_get_app_info(void);
