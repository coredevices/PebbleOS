/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "system/status_codes.h"

// motor full power
#define VIBE_STRENGTH_MAX 100
// motor full reverse
#define VIBE_STRENGTH_MIN -100
// motor stopped
#define VIBE_STRENGTH_OFF 0

void vibe_init(void);
void vibe_ctl(bool on);
void vibe_force_off(void);
void vibe_set_strength(int8_t strength);

//! Return the strength that should be used for braking the motor to a stop.
int8_t vibe_get_braking_strength(void);

//! Calibrate the vibe motor driver for optimal performance.
//!
//! If vibration motor has NV memory, calibration data will be stored there on
//! a successful calibration.
//!
//! @retval S_SUCCESS indicating success or failure of calibration.
//! @retval error Code indicating failure of calibration.
status_t vibe_calibrate(void);
