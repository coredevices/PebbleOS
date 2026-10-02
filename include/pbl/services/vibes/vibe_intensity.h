/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup services_vibes_vibe_intensity Vibe intensity
 * @ingroup services_vibes
 * @brief Default strength of all vibrations.
 * @{
 */

/** @brief Vibration intensity. */
typedef enum VibeIntensity {
  /** Low, 40 % strength. */
  VibeIntensityLow,
  /** Medium, 60 % strength. */
  VibeIntensityMedium,
  /** High, full strength. */
  VibeIntensityHigh,
  /** Number of intensities. */
  VibeIntensityNum,
} VibeIntensity;

/** @brief Intensity used when the user has not chosen one. */
#define DEFAULT_VIBE_INTENSITY VibeIntensityHigh

/** @brief Apply the intensity stored in the alert preferences. */
void vibe_intensity_init(void);

/**
 * @brief Get the strength of an intensity.
 *
 * @param intensity Intensity.
 * @return Percentage of the maximum strength, 0-100. Unknown intensities map to 100.
 */
uint8_t get_strength_for_intensity(VibeIntensity intensity);

/**
 * @brief Set the default strength of all vibrations (not just notifications).
 *
 * Does not persist the intensity.
 *
 * @param intensity Intensity to apply.
 */
void vibe_intensity_set(VibeIntensity intensity);

/**
 * @brief Get the intensity stored in the alert preferences.
 *
 * @return Current intensity.
 */
VibeIntensity vibe_intensity_get(void);

/**
 * @brief Get the display name of an intensity.
 *
 * @param intensity Intensity.
 * @return Untranslated name, to be passed through i18n, or NULL if @p intensity is invalid.
 */
const char *vibe_intensity_get_string_for_intensity(VibeIntensity intensity);

/**
 * @brief Get the next intensity in the cycle, wrapping around.
 *
 * @param intensity Current intensity.
 * @return Next intensity.
 */
VibeIntensity vibe_intensity_cycle_next(VibeIntensity intensity);

/** @} */
