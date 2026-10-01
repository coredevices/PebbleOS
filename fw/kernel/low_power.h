/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Standby while in Low Power Mode
void low_power_standby(void);

//! Enter low power state
void low_power_enter(void);

//! Leave low power state
void low_power_exit(void);

//! Get low power state
bool low_power_is_active(void);
