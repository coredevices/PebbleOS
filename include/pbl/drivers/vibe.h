/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <system/status_codes.h>

/**
 * @defgroup drivers_vibe Vibration motor
 * @ingroup drivers
 * @brief Vibration motor driver interface.
 *
 * @code{.c}
 * vibe_set_strength(VIBE_STRENGTH_MAX);
 * vibe_ctl(true);
 * // ...
 * vibe_ctl(false);
 * @endcode
 * @{
 */

/** @brief Full power. */
#define VIBE_STRENGTH_MAX 100
/** @brief Full reverse, for drivers that can brake. */
#define VIBE_STRENGTH_MIN -100
/** @brief Stopped. */
#define VIBE_STRENGTH_OFF 0

/** @brief Initialize the vibration motor driver. */
void vibe_init(void);
/**
 * @brief Start or stop the motor at the strength set with vibe_set_strength().
 *
 * @param on true to start.
 */
void vibe_ctl(bool on);
/** @brief Stop the motor immediately. */
void vibe_force_off(void);
/**
 * @brief Set the drive strength.
 *
 * Drivers without reverse drive use the absolute value.
 *
 * @param strength Strength from @ref VIBE_STRENGTH_MIN to @ref VIBE_STRENGTH_MAX.
 */
void vibe_set_strength(int8_t strength);

/**
 * @brief Get the strength to use to brake the motor to a stop.
 *
 * @return Braking strength, @ref VIBE_STRENGTH_OFF if the driver does not brake this way.
 */
int8_t vibe_get_braking_strength(void);

/**
 * @brief Calibrate the motor driver.
 *
 * If the driver has non-volatile memory, a successful calibration is stored there.
 *
 * @retval S_SUCCESS Calibration succeeded.
 * @return Otherwise an error code.
 */
status_t vibe_calibrate(void);

/**
 * @brief Get the calibration computed by the last successful vibe_calibrate().
 *
 * @return Driver-specific calibration value, to persist to non-volatile storage.
 */
uint8_t vibe_get_calibration(void);

/**
 * @brief Apply a calibration value previously returned by vibe_get_calibration().
 *
 * @param cali Calibration value.
 */
void vibe_apply_calibration(uint8_t cali);

/** @} */
