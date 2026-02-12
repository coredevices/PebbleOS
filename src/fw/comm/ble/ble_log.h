/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/logging.h"
#include "system/passert.h"
#include "system/hexdump.h"

#define BLE_LOG_DEBUG(fmt, args...) PBL_LOG_D_DBG(LOG_DOMAIN_BT, fmt, ## args)
#define BLE_LOG_VERBOSE(fmt, args...) PBL_LOG_D_VERBOSE(LOG_DOMAIN_BT, fmt, ## args)
#define BLE_HEXDUMP(data, length) PBL_HEXDUMP_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, data, length)
#define BLE_HEXDUMP_VERBOSE(data, length) PBL_HEXDUMP_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG_VERBOSE, data, length)
