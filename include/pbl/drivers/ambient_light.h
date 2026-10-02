/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_ambient_light Ambient light sensor
 * @ingroup drivers
 * @brief Ambient light sensor (ALS) driver interface.
 *
 * Readings are raw counts between 0 and #AMBIENT_LIGHT_LEVEL_MAX. Boards with calibration
 * coefficients can convert them to lux with ambient_light_level_to_lux().
 *
 * Sampling is controlled by two reference counts kept by the common code: a client that will
 * read the sensor soon primes it, and code that would disturb the reading (e.g. backlight
 * bleed-through) suspends it. The sensor samples while primed and not suspended.
 *
 * @code{.c}
 * ambient_light_prime();
 * ...
 * uint32_t lux = ambient_light_level_to_lux(ambient_light_get_light_level());
 * ambient_light_release();
 * @endcode
 * @{
 */

/** @brief Light level enum */
typedef enum AmbientLightLevel {
  /** No reading available. */
  AmbientLightLevelUnknown = 0,
  /** Well below the dark threshold. */
  AmbientLightLevelVeryDark,
  /** Below the dark threshold. */
  AmbientLightLevelDark,
  /** At or slightly above the dark threshold. */
  AmbientLightLevelLight,
  /** Well above the dark threshold. */
  AmbientLightLevelVeryLight,
} AmbientLightLevel;

/** @brief Number of AmbientLightLevel values. */
#define AMBIENT_LIGHT_LEVEL_ENUM_COUNT (AmbientLightLevelVeryLight + 1)

/** @cond INTERNAL_HIDDEN */
#ifndef CONFIG_AMBIENT_LIGHT_BITS
// Fallback for header parsers (e.g. SDK generator) that don't preload autoconf.h.
#define CONFIG_AMBIENT_LIGHT_BITS 12
#endif
/** @endcond */

/** @brief Upper bound of the raw light level scale (@c CONFIG_AMBIENT_LIGHT_BITS bits). */
static const uint32_t AMBIENT_LIGHT_LEVEL_MAX = (1U << CONFIG_AMBIENT_LIGHT_BITS);

/** @brief Initialize the ambient light sensor. */
void ambient_light_init(void);

/**
 * @brief Read the light level.
 *
 * @return Raw light level between 0 and #AMBIENT_LIGHT_LEVEL_MAX, 0 if the sensor is not
 *         initialized.
 */
uint32_t ambient_light_get_light_level(void);

/**
 * @brief Announce that readings will be wanted soon.
 *
 * Reference counted; balance with ambient_light_release(). Lets drivers that sample
 * continuously start ahead of the first read.
 */
void ambient_light_prime(void);

/** @brief Drop a reference taken with ambient_light_prime(). */
void ambient_light_release(void);

/**
 * @brief Stop sampling until the matching ambient_light_resume().
 *
 * Reference counted. Used while something disturbs the sensor, e.g. backlight bleed-through.
 */
void ambient_light_suspend(void);

/** @brief Drop a reference taken with ambient_light_suspend(). */
void ambient_light_resume(void);

/**
 * @brief Initialize the reference counting common to all drivers.
 *
 * Called by the driver's ambient_light_init(); prime and suspend requests made earlier are
 * ignored.
 */
void ambient_light_common_init(void);

/**
 * @brief Apply the sampling state computed by the common code.
 *
 * Implemented by the driver and called with the common lock held whenever a reference count
 * changes. A no-op for drivers without a sampling gate.
 *
 * @param active True while primed.
 * @param sampling True while primed and not suspended.
 */
void ambient_light_driver_set_state(bool active, bool sampling);

/**
 * @brief Get the threshold between light and dark.
 *
 * @return Threshold, in the units returned by ambient_light_level_to_lux().
 */
uint32_t ambient_light_get_dark_threshold(void);

/**
 * @brief Set the threshold between light and dark.
 *
 * @param new_threshold Threshold, in the units returned by ambient_light_level_to_lux(), at most
 *                      #AMBIENT_LIGHT_LEVEL_MAX.
 */
void ambient_light_set_dark_threshold(uint32_t new_threshold);

/**
 * @brief Check whether it is light.
 *
 * @return True if the current level, converted with ambient_light_level_to_lux(), is above the
 *         dark threshold; false if dark or the sensor is unavailable.
 */
bool ambient_light_is_light();

/**
 * @brief Classify a light level against the dark threshold.
 *
 * @param light_level Light level, in the units of the dark threshold.
 * @return Light level class, AmbientLightLevelUnknown if the sensor is unavailable.
 */
AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level);

/**
 * @brief Check whether the board has raw-count to lux coefficients.
 *
 * @return True if ambient_light_level_to_lux() converts to lux.
 */
bool ambient_light_lux_available(void);

/**
 * @brief Convert a light level to lux using the board coefficients.
 *
 * On boards without coefficients the level is returned unchanged, so callers can use the
 * result unconditionally.
 *
 * @param light_level Light level, after screen compensation if any.
 * @return Light level in lux, or @p light_level if uncalibrated.
 */
uint32_t ambient_light_level_to_lux(uint32_t light_level);

/** @} */
