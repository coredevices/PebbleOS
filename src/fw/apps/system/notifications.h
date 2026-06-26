/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

// UUID: d9c0d758-54bd-45b1-99ac-2a3a889350c9
#define NOTIFICATIONS_CLEAR_HISTORY_UUID {0xd9, 0xc0, 0xd7, 0x58, 0x54, 0xbd, 0x45, 0xb1, \
                                          0x99, 0xac, 0x2a, 0x3a, 0x88, 0x93, 0x50, 0xc9}

const PebbleProcessMd* notifications_app_get_info();
const PebbleProcessMd* notifications_clear_history_app_get_info(void);
