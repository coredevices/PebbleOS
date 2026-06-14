/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gap_le_advert.h"

//! @note The client should hold the `bt_lock` when calling these functions.
//! Also note that these functions are a workaround and should ideally not be used.
//! They are kept around to assist with a bug in the TI Bluetooth chips.

extern GAPLEAdvertisingJobRef gap_le_advert_get_current_job(void);

extern GAPLEAdvertisingJobRef gap_le_advert_get_jobs(void);

extern GAPLEAdvertisingJobTag gap_le_advert_get_job_tag(GAPLEAdvertisingJobRef job);
