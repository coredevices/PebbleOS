/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef enum VibeIntensity {
  VibeIntensityLow,
  VibeIntensityMedium,
  VibeIntensityHigh,
  VibeIntensityNum,
} VibeIntensity;

#if PLATFORM_SPALDING
#define DEFAULT_VIBE_INTENSITY VibeIntensityMedium
#else
#define DEFAULT_VIBE_INTENSITY VibeIntensityHigh
#endif

void vibe_intensity_init(void);

//! Returns the corresponding strength for the given level of intensity.
//! The strength corresponds to a percentage of the max strength, ie in the range [0,100].
uint8_t get_strength_for_intensity(VibeIntensity intensity);

//! Sets the intensity of ALL vibrations (not just notifications)
void vibe_intensity_set(VibeIntensity intensity);

//! Gets the current vibe intensity
VibeIntensity vibe_intensity_get(void);

//! Returns a string representation of the provided vibe intensity.
//! @param intensity The intensity for which to get a string representation
//! @return A string representation of the provided intensity, or NULL if the intensity is invalid
const char *vibe_intensity_get_string_for_intensity(VibeIntensity intensity);

//! Gets the next intensity in the vibe intensity cycle
//! @param intensity Input intensity for which to get the next intensity
//! @return The next vibe intensity in the cycle
VibeIntensity vibe_intensity_cycle_next(VibeIntensity intensity);
