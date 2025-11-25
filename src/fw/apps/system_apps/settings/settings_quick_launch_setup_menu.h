/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "settings_quick_launch.h"

#include "process_management/pebble_process_md.h"
#include "shell/normal/quick_launch.h"

#include <stdbool.h>

const PebbleProcessMd* quick_launch_setup_get_app_info(void);
