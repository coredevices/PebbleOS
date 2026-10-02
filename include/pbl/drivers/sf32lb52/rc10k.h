/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup drivers_sf32lb52 SF32LB52
 * @ingroup drivers
 * @brief SF32LB52-specific driver definitions.
 * @{
 */

/**
 * @brief Initialize the RC10K low-power oscillator calibration.
 *
 * Calibrates the oscillator against the 48 MHz crystal now and then every 15 seconds.
 */
void rc10k_init(void);

/**
 * @brief Get the calibrated RC10K frequency.
 *
 * @return Frequency in Hz, nominal 10 kHz if no calibration is available.
 */
uint32_t rc10k_get_freq_hz(void);

/**
 * @brief Convert RC10K cycles to RTC milli-ticks.
 *
 * @param rc10k_cyc Number of RC10K cycles.
 * @return Duration in thousandths of an RTC tick.
 */
uint32_t rc10k_cyc_to_milli_ticks(uint32_t rc10k_cyc);

/** @} */
