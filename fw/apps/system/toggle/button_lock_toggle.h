/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <process_management/app_manager.h>

// UUID: eea080f7-db58-44b4-a23b-f590b57053a7
#define BUTTON_LOCK_TOGGLE_UUID \
  {0xee, 0xa0, 0x80, 0xf7, 0xdb, 0x58, 0x44, 0xb4, 0xa2, 0x3b, 0xf5, 0x90, 0xb5, 0x70, 0x53, 0xa7}

const PebbleProcessMd *button_lock_toggle_get_app_info(void);
