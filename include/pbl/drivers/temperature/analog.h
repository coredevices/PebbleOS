/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup drivers_temperature_analog Analog temperature sensor
 * @ingroup drivers_temperature
 * @brief Board description of a temperature sensor read through a voltage monitor.
 *
 * The temperature is linear in the measured voltage: it equals @c millidegrees_ref at
 * @c millivolts_ref, with slope @c slope_numerator / @c slope_denominator.
 * @{
 */

/** @brief Analog temperature sensor description. */
struct AnalogTemperatureSensor {
  /** Voltage monitor measuring the sensor output. */
  const VoltageMonitorDevice *voltage_monitor;
  /** Reference voltage in millivolts. */
  int32_t millivolts_ref;
  /** Temperature at the reference voltage, in millidegrees Celsius. */
  int32_t millidegrees_ref;
  /** Slope numerator. */
  int32_t slope_numerator;
  /** Slope denominator. */
  int32_t slope_denominator;
};

/** @} */
