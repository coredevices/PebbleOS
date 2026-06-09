/* SPDX-FileCopyrightText: 2026 Shashvat Prabhu */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/battery/battery_state.h"

// The battery charge limit service optionally stops charging at 80% to reduce battery
// degradation from sustained high charge levels. Charging resumes if the level drops back
// below 80%.

void battery_charge_limit_init(void);

void battery_charge_limit_evaluate(PreciseBatteryChargeState state);

bool battery_charge_limit_is_active(void);
