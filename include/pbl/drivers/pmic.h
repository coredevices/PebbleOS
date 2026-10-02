/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
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
 * @brief Initialize the PMIC driver.
 *
 * Called once at startup.
 *
 * @return true on success.
 */
bool pmic_init(void);

/**
 * @brief Power off the board.
 *
 * All components lose power except the RTC, and the PMIC wakes the board on a button press.
 * Fails while USB power is connected.
 *
 * @return false if the board did not power off.
 */
bool pmic_power_off(void);

/**
 * @brief Enable the PMIC battery monitor.
 *
 * Disable it with pmic_disable_battery_measure() when readings are no longer needed.
 *
 * @return true on success.
 */
bool pmic_enable_battery_measure(void);

/**
 * @brief Disable the PMIC battery monitor.
 *
 * @return true on success.
 */
bool pmic_disable_battery_measure(void);

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
 * @brief Read chip information for tracking purposes.
 *
 * @param[out] chip_id Chip ID.
 * @param[out] chip_revision Chip revision.
 * @param[out] buck1_vset BUCK1 voltage setting.
 */
void pmic_read_chip_info(uint8_t *chip_id, uint8_t *chip_revision, uint8_t *buck1_vset);

/**
 * @brief Measure VSYS.
 *
 * @return VSYS in millivolts, or 0 on error.
 */
uint16_t pmic_get_vsys(void);

/*
 * FIXME: The following functions are unrelated to the PMIC and should be moved to the
 * display/accessory connector drivers once we have them.
 */

/**
 * @brief Switch the LDO3 power rail.
 *
 * @param enabled true to power the rail.
 */
void set_ldo3_power_state(bool enabled);

/**
 * @brief Switch the 4.5 V power rail.
 *
 * @param enabled true to power the rail.
 */
void set_4V5_power_state(bool enabled);

/**
 * @brief Switch the 6.6 V power rail.
 *
 * @param enabled true to power the rail.
 */
void set_6V6_power_state(bool enabled);

/** @} */
