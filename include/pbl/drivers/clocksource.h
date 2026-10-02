/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_clocksource Clock sources
 * @ingroup drivers
 * @brief Clock source requests.
 * @{
 */

#ifdef CONFIG_SOC_NRF52
/**
 * @brief Request the high-frequency crystal oscillator (HFXO).
 *
 * Reference counted; balance with clocksource_hfxo_release(). Starts the oscillator on the
 * first request and busy-waits until it runs. nRF52 only.
 */
void clocksource_hfxo_request(void);

/**
 * @brief Release the high-frequency crystal oscillator.
 *
 * Stops the oscillator when the last request is released. nRF52 only.
 */
void clocksource_hfxo_release(void);

#endif

/** @} */
