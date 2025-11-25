/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#if MICRO_FAMILY_STM32F2 || MICRO_FAMILY_STM32F4 || MICRO_FAMILY_STM32F7

//! Configure and start the 32 kHz LSE clock.
void clocksource_lse_configure(void);

//! Returns true iff LSE is running.
bool clocksource_is_lse_started(void);

//! Enable or disable 32 kHz clock output on pin MCO1.
//!
//! Prerequisite: clocksource_lse_configure() must be called beforehand to
//! enable the 32 kHz clock.
void clocksource_MCO1_enable(bool on);

#endif

#if MICRO_FAMILY_NRF5

/** @brief Request HFXO clock (reference counted). */
void clocksource_hfxo_request(void);

/** @brief Release HFXO clock (reference counted). */
void clocksource_hfxo_release(void);

#endif
