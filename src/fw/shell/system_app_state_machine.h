/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"
#include "process_management/app_install_types.h"

//! Get the very first app we should run at startup.
const PebbleProcessMd* system_app_state_machine_system_start(void);

//! Retrieve the app that was last launched prior to the current one.
AppInstallId system_app_state_machine_get_last_registered_app(void);

//! Get the app we should launch after an app has crashed or has been force quit.
const PebbleProcessMd* system_app_state_machine_get_default_app(void);

//! Tell the state machine we just launched an app.
void system_app_state_machine_register_app_launch(AppInstallId app_id);

//! Stop the normal state machine! Just show the panic app.
void system_app_state_machine_panic(void);

