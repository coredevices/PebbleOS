/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#if MICRO_FAMILY_NRF52

/** @brief Request HFXO clock (reference counted). */
void clocksource_hfxo_request(void);

/** @brief Release HFXO clock (reference counted). */
void clocksource_hfxo_release(void);

#endif
