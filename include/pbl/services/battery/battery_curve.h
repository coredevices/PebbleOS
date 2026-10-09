/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdint.h>

/**
 * @defgroup services_battery_battery_curve Battery curve
 * @ingroup services_battery
 * @brief Conversion between battery voltage, charge percentage and time remaining.
 *
 * The voltage curve backend uses per-board charge and discharge curves; with the nRF fuel gauge
 * only the time remaining conversions are functional.
 * @{
 */

/** @brief Source of a voltage compensation. */
typedef enum {
  /** Load of the status LED. */
  BATTERY_CURVE_COMPENSATE_STATUS_LED,
  /** Number of compensation sources. */
  BATTERY_CURVE_COMPENSATE_COUNT
} BatteryCurveVoltageCompensationKey;

/**
 * @brief Set a compensation added to the measured voltage when computing the charge.
 *
 * For example, a constantly lit LED makes the measured voltage drop due to the internal
 * resistance of the battery.
 *
 * @param key Compensation source.
 * @param mv Compensation in mV.
 */
void battery_curve_set_compensation(BatteryCurveVoltageCompensationKey key, int mv);

/**
 * @brief Move the 100 % point of the discharge curve.
 *
 * Clamped so it stays above the next point of the curve.
 *
 * @param voltage New 100 % voltage in mV.
 */
void battery_curve_set_full_voltage(uint16_t voltage);

#if UNITTEST
/**
 * @brief Restore the discharge curve changed by battery_curve_set_full_voltage().
 *
 * Unit tests only.
 */
void battery_curve_reset_for_tests(void);
#endif

/**
 * @brief Get the charge for a voltage, applying the compensations.
 *
 * @param battery_mv Battery voltage in mV.
 * @param is_charging true to use the charge curve.
 * @return Charge as a ratio32.
 */
uint32_t battery_curve_sample_ratio32_charge_percent(uint32_t battery_mv, bool is_charging);

/**
 * @brief Get the charge for a voltage, scaled by a factor, without compensations.
 *
 * Interpolates linearly between curve points and clamps to the curve ends.
 *
 * @param battery_mv Battery voltage in mV.
 * @param is_charging true to use the charge curve.
 * @param scaling_factor Value of 1 %.
 * @return Charge in units of @p scaling_factor per percent.
 */
int32_t battery_curve_lookup_percent_with_scaling_factor(int battery_mv, bool is_charging,
                                                         uint32_t scaling_factor);

/**
 * @brief Get the hours of use left before low power mode.
 *
 * @param percent_remaining Charge in percent, including the low power reserve.
 * @return Hours remaining.
 */
uint32_t battery_curve_get_hours_remaining(uint32_t percent_remaining);

/**
 * @brief Get the charge needed for some hours of use before low power mode.
 *
 * Inverse of battery_curve_get_hours_remaining().
 *
 * @param hours Hours of use.
 * @return Charge in percent, including the low power reserve.
 */
uint32_t battery_curve_get_percent_remaining(uint32_t hours);

/**
 * @brief Get the voltage for a charge percentage.
 *
 * Used by unit tests and QEMU.
 *
 * @param percent Charge in percent.
 * @param is_charging true to use the charge curve.
 * @return Battery voltage in mV.
 */
uint32_t battery_curve_lookup_voltage_by_percent(uint32_t percent, bool is_charging);

/** @} */
