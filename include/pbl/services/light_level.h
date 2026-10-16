/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup services_light_level Light level
 * @ingroup services_light
 * @brief Coarse ambient light level, classified against the backlight dark threshold.
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

/**
 * @brief Classify a light level against the dark threshold.
 *
 * @param lux Light level, in the units of the dark threshold.
 * @return Light level class.
 */
AmbientLightLevel light_level_to_enum(uint32_t lux);

/** @} */
