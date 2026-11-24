/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>
#include <pbl/services/wakeup.h>

#include <process_management/app_install_types.h>
#include <process_management/launch_config.h>
#include <process_management/pebble_process_md.h>

typedef struct AppFetchUIArgs {
  LaunchConfigCommon common;
  WakeupInfo wakeup_info;
  AppInstallId app_id;
  bool forcefully;     //!< whether to launch forcefully or not
  bool kill_requested; //!< whether the forced close was requested
} AppFetchUIArgs;

//! The fetch screen is a gift box scene on color displays, whose resources carry its frames
#ifdef CONFIG_PLATFORM_COLOR
#define APP_FETCH_UI_GIFT_SUPPORTED 1
//! What the screen ends filled with, which the launch transition starts from
#define APP_FETCH_UI_FILL_COLOR system_theme_get_highlight_color()
#else
#define APP_FETCH_UI_GIFT_SUPPORTED 0
#define APP_FETCH_UI_FILL_COLOR     GColorLightGray
#endif

//! Used to launch the app_fetch_ui application
const PebbleProcessMd *app_fetch_ui_get_app_info(void);
