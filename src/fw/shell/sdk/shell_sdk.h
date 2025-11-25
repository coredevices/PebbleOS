/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_management/app_install_types.h"

//! @file shell_sdk.h
//!
//! Hooks into system_app_state_machine.c to watch for installed apps to be launched. Latches
//! so we can figure out what was the installed app that we've launched most recently.

AppInstallId shell_sdk_get_last_installed_app(void);

void shell_sdk_set_last_installed_app(AppInstallId app_id);

bool shell_sdk_last_installed_app_is_watchface();
