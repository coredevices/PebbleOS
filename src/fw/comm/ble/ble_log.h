/*
 * Copyright 2024 Google LLC
 *
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

#define FILE_LOG_COLOR LOG_COLOR_BLUE
#include "system/logging.h"
#include "system/passert.h"
#include "system/hexdump.h"

#define BLE_LOG_DEBUG(fmt, args...) PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, fmt, ## args)
#define BLE_LOG_VERBOSE(fmt, args...) PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG_VERBOSE, fmt, ## args)
#define BLE_HEXDUMP(data, length) PBL_HEXDUMP_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, data, length)

