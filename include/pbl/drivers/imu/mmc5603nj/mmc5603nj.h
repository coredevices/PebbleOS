/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-FileCopyrightText: 2025 Bob Wei */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_imu_mmc5603nj MMC5603NJ
 * @ingroup drivers_imu
 * @brief MEMSIC MMC5603NJ magnetometer driver.
 *
 * Implements @ref drivers_mag. Samples are reported in milligauss, with the axis mapping of
 * the board's magnetometer configuration.
 * @{
 */

/** @brief Initialize the MMC5603NJ. */
void mmc5603nj_init(void);

/**
 * @brief Set whether the watch is mounted rotated 180 degrees.
 *
 * When rotated, the X and Y axes of subsequent samples are inverted.
 *
 * @param rotated true if rotated.
 */
void mag_set_rotated(bool rotated);

/** @} */
