/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup drivers_pmic PMIC
 * @ingroup drivers
 * @brief Power management IC: power off, charger and supply monitoring.
 *
 * Functions returning bool report whether the PMIC could be accessed.
 * @{
 */

/**
 * @brief Enable or disable battery charging.
 *
 * @param enable true to enable charging.
 * @return true on success.
 */
bool pmic_set_charger_state(bool enable);

/**
 * @brief Check whether the battery is being charged.
 *
 * Once the battery is full it is no longer charging, even with the charger connected (see
 * pmic_is_usb_connected()).
 *
 * @return true while charging.
 */
bool pmic_is_charging(void);

/**
 * @brief Check whether a USB charger is connected.
 *
 * @return true if VBUS is present.
 */
bool pmic_is_usb_connected(void);

/**
 * @brief Measure VSYS.
 *
 * @return VSYS in millivolts, or 0 on error.
 */
uint16_t pmic_get_vsys(void);

/** @} */
