/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

const PebbleProcessMd *simplicity_get_app_info(void);
const PebbleProcessMd *low_power_face_get_app_info(void);
const PebbleProcessMd *music_app_get_info(void);
const PebbleProcessMd *notifications_app_get_info(void);
const PebbleProcessMd *alarms_app_get_info(void);
const PebbleProcessMd *watchfaces_get_app_info(void);
const PebbleProcessMd *settings_get_app_info(void);
const PebbleProcessMd *recovery_first_use_app_get_app_info(void);
const PebbleProcessMd *set_time_get_app_info(void);
const PebbleProcessMd *quick_launch_setup_get_app_info(void);
const PebbleProcessMd *quiet_time_toggle_get_app_info(void);
