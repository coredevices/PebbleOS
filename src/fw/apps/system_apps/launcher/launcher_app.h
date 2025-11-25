/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if PLATFORM_TINTIN
#include "legacy/launcher_app.h"
#else
#include "default/launcher_app.h"
#endif

#include "process_management/pebble_process_md.h"

#define RETURN_TIMEOUT_TICKS (5 * RTC_TICKS_HZ)

const PebbleProcessMd* launcher_menu_app_get_app_info(void);
