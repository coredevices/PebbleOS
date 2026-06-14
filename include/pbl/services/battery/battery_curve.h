/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

// Handles battery mV <-> % conversion

typedef enum {
  BATTERY_CURVE_COMPENSATE_STATUS_LED,
  BATTERY_CURVE_COMPENSATE_COUNT
} BatteryCurveVoltageCompensationKey;

//! Set compensation value to be applied to battery voltage when calculating percentage charge.
//! For example, if an LED is constantly on, the voltage being measured is going to drop due to the
//! internal resistance of the battery.
void battery_curve_set_compensation(BatteryCurveVoltageCompensationKey key, int mv);

void battery_curve_set_full_voltage(uint16_t voltage);

//! Returns the corresponding battery percentage as a ratio32.
uint32_t battery_curve_sample_ratio32_charge_percent(uint32_t battery_mv, bool is_charging);

uint32_t battery_curve_lookup_percent_by_voltage(uint32_t battery_mv, bool is_charging);

int32_t battery_curve_lookup_percent_with_scaling_factor(
    int battery_mv, bool is_charging, uint32_t scaling_factor);

uint32_t battery_curve_get_hours_remaining(uint32_t percent_remaining);

uint32_t battery_curve_get_percent_remaining(uint32_t hours);

// This is used by unit tests and QEMU
uint32_t battery_curve_lookup_voltage_by_percent(uint32_t percent, bool is_charging);
