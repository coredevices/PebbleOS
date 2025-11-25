/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <bluetooth/bluetooth_types.h>

bool fake_HCIAPI_whitelist_contains(const BTDeviceInternal *device);

uint32_t fake_HCIAPI_whitelist_count(void);

uint32_t fake_HCIAPI_whitelist_error_count(void);

void fake_HCIAPI_deinit(void);
