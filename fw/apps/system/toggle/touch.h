/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"

#define TOUCH_TOGGLE_UUID \
  {0x5c, 0xc5, 0xa4, 0xe8, 0x15, 0x0d, 0x4e, 0xc3, 0x9a, 0xfc, 0x96, 0x5e, 0x3f, 0xde, 0xc3, 0xc3}

const PebbleProcessMd *touch_toggle_get_app_info(void);
