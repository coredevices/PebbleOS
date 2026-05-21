/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/battery/battery_state.h"

// The battery charge limit service optionally stops charging at 80% and resumes at 77%
// to reduce battery degradation from sustained high charge levels.

void battery_charge_limit_init(void);

void battery_charge_limit_evaluate(PreciseBatteryChargeState state);

bool battery_charge_limit_is_active(void);
