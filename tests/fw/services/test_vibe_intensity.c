/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/vibes/vibe_intensity.h"

// Stubs
/////////

#include "stubs_alerts_preferences.h"
#include "stubs_vibe_pattern.h"

// Setup and Teardown
//////////////////////

void test_vibe_intensity__initialize(void) {
}

void test_vibe_intensity__cleanup(void) {
}

// Tests
/////////

void test_vibe_intensity__get_string_for_intensity(void) {
  // A bogus intensity returns NULL
  cl_assert_equal_p(vibe_intensity_get_string_for_intensity(VibeIntensityNum), NULL);

  cl_assert_equal_s(vibe_intensity_get_string_for_intensity(VibeIntensityLow), "Standard - Low");
  cl_assert_equal_s(vibe_intensity_get_string_for_intensity(VibeIntensityMedium),
                    "Standard - Medium");
  cl_assert_equal_s(vibe_intensity_get_string_for_intensity(VibeIntensityHigh), "Standard - High");
}

void test_vibe_intensity__cycle_next(void) {
  // Low -> Medium
  cl_assert(vibe_intensity_cycle_next(VibeIntensityLow) == VibeIntensityMedium);

  // Medium -> High
  cl_assert(vibe_intensity_cycle_next(VibeIntensityMedium) == VibeIntensityHigh);

  // High -> Low
  cl_assert(vibe_intensity_cycle_next(VibeIntensityHigh) == VibeIntensityLow);
}
