/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

//! Call for system to obtain information about the application
//! @return System information about the app
const PebbleProcessMd *health_app_get_info(void);
