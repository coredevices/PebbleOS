/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"
#include "applib/ui/ui.h"

#include <stdbool.h>

const PebbleProcessMd* mfg_display_burnin_get_app_info();

const Uuid* mfg_display_burnin_get_uuid();
