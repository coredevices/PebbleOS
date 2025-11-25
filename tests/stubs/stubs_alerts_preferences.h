/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/notifications/alerts_preferences_private.h"
#include "services/normal/vibes/vibe_intensity.h"

VibeIntensity alerts_preferences_get_vibe_intensity(void) {
  return DEFAULT_VIBE_INTENSITY;
}
