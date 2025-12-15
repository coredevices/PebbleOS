/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"
#include <stdint.h>

//! Initialize the watchface metrics module
void watchface_metrics_init(void);

//! Called when a watchface starts running. If a different watchface was
//! previously tracked, the time resets to 0.
//! @param uuid The UUID of the watchface
void watchface_metrics_start(const Uuid *uuid);

//! Called when a watchface stops running. Persists accumulated time.
void watchface_metrics_stop(void);

//! Get the total accumulated time for the current watchface.
//! Time resets when switching to a different watchface.
//! @return Total time in seconds
uint32_t watchface_metrics_get_current_time(void);
