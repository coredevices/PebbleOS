/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <process_management/app_manager.h>

// UUID: e8b3a69f-a457-4a77-9e1d-996d8e2671c5
#define BUTTON_LOCK_AUTO_TOGGLE_UUID \
  {0xe8, 0xb3, 0xa6, 0x9f, 0xa4, 0x57, 0x4a, 0x77, 0x9e, 0x1d, 0x99, 0x6d, 0x8e, 0x26, 0x71, 0xc5}

const PebbleProcessMd *button_lock_auto_toggle_get_app_info(void);
