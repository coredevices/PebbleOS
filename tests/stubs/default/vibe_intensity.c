/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/vibes/vibe_intensity.h>

PBL_WEAK uint8_t get_strength_for_intensity(VibeIntensity intensity) {
  return 0;
}

PBL_WEAK VibeIntensity vibe_intensity_get(void) {
  return VibeIntensityLow;
}

PBL_WEAK void vibe_intensity_set(VibeIntensity intensity) {
}
