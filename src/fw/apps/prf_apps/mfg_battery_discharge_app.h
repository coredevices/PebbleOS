/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

//! Get the process metadata for the Battery Discharge Test MFG app
//! @return Pointer to the process metadata structure
const PebbleProcessMd* mfg_battery_discharge_app_get_info(void);
