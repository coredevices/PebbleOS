/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Factory resets the device by wiping the flash
//! @param should_shutdown If true, shutdown after factory resetting, otherwise reboot.
void factory_reset(bool should_shutdown);

//! Factory resets the device by deleting all files
void factory_reset_fast(void *unused);

//! Returns true if a factory reset is in progress.
bool factory_reset_ongoing(void);
