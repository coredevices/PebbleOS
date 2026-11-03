/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/services/time.h>

// Timezone storage that survives resets.

void tz_storage_save(const TimezoneInfo *tz_info);

void tz_storage_load(TimezoneInfo *tz_info);

bool tz_storage_is_set(void);

void tz_storage_clear(void);
