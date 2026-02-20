/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"

#define QUIET_TIME_TOGGLE_UUID {0x22, 0x20, 0xd8, 0x05, 0xcf, 0x9a, 0x4e, 0x12, \
                                0x92, 0xb9, 0x5c, 0xa7, 0x78, 0xaf, 0xf6, 0xbb}

const PebbleProcessMd *quiet_time_toggle_get_app_info(void);
