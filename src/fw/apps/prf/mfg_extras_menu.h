/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

const PebbleProcessMd* mfg_extras_menu_app_get_info(void);

//! Check and clear the relaunch flag (set when returning from an extras item)
bool mfg_extras_menu_should_relaunch(void);
