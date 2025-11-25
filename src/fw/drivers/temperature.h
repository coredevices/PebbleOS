/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void temperature_init(void);

// Get the temperature in millidegrees-C
// WARNING: the temperature sensor may not be calibrated and thus this reading
// should not be relied on to get a true representation of absolute temperature
int32_t temperature_read(void);
