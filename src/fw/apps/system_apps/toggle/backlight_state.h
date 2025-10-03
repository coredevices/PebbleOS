/*
 * Copyright 2025 Elad Dvash
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "process_management/app_manager.h"

#define BACKLIGHT_STATE_TOGGLE_UUID {0xd0, 0xf1, 0x2e, 0x6c, 0x97, 0xeb, 0x22, 0x87, \
                                      0xa2, 0xf5, 0x11, 0x5d, 0xfa, 0xA1, 0xd1, 0x68}

const PebbleProcessMd *backlight_state_toggle_get_app_info(void);
