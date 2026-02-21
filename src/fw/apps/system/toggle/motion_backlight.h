/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"

#define MOTION_BACKLIGHT_TOGGLE_UUID {0xd4, 0xf7, 0xbe, 0x63, 0x97, 0xe6, 0x49, 0x52, \
                                      0xb2, 0x65, 0xdd, 0x4b, 0xce, 0x11, 0xc1, 0x55}

const PebbleProcessMd *motion_backlight_toggle_get_app_info(void);
