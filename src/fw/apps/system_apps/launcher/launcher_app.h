/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "default/launcher_app.h"

#include "process_management/pebble_process_md.h"

#define RETURN_TIMEOUT_TICKS (5 * RTC_TICKS_HZ)

const PebbleProcessMd* launcher_menu_app_get_app_info(void);
