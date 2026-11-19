/* SPDX-FileCopyrightText: 2026 Shashvat Prabhu */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/services/battery/battery_state.h>

// The battery charge limit service optionally stops charging at a user set level to reduce
// battery degradation from sustained high charge levels. While plugged in with charging stopped,
// the watch runs from USB power. Charging resumes if the level drops below the limit, and the
// charger is always re-enabled on unplug or when the limit is turned off.

//! Pref value for no charge limit.
#define CHARGE_LIMIT_PCT_OFF 0
//! Lowest accepted charge limit, in percent.
#define CHARGE_LIMIT_PCT_MIN 50
//! Highest accepted charge limit, in percent.
#define CHARGE_LIMIT_PCT_MAX 95

void battery_charge_limit_evaluate(PreciseBatteryChargeState state);

bool battery_charge_limit_is_active(void);
