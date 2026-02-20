/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"

#define AIRPLANE_MODE_TOGGLE_UUID {0x88, 0xc2, 0x8c, 0x12, 0x7f, 0x81, 0x42, 0xdb, \
                                   0xaa, 0xa6, 0x14, 0xcc, 0xef, 0x6f, 0x27, 0xe5}

const PebbleProcessMd *airplane_mode_toggle_get_app_info(void);
