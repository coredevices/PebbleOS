/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "applib/accel_service.h"

void fake_accel_service_invoke_callbacks(AccelData *data, uint32_t num_samples);

