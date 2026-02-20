/* SPDX-FileCopyrightText: 2025 Elad Dvash */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"

#define BACKLIGHT_STATE_TOGGLE_UUID {0xd0, 0xf1, 0x2e, 0x6c, 0x97, 0xeb, 0x22, 0x87, \
                                      0xa2, 0xf5, 0x11, 0x5d, 0xfa, 0xA1, 0xd1, 0x68}

const PebbleProcessMd *backlight_state_toggle_get_app_info(void);
