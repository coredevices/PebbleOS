/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum VibeIntensity {
  VibeIntensity_Stub
} VibeIntensity;

#define DEFAULT_VIBE_INTENSITY VibeIntensity_Stub

uint8_t get_strength_for_intensity(VibeIntensity intensity) {
  return 0;
}

VibeIntensity vibe_intensity_get(void) {
  return VibeIntensity_Stub;
}

void vibe_intensity_set(VibeIntensity intensity) {}
