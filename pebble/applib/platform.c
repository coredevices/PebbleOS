/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "platform.h"

#include <pbl/util/bits.h>

#include <process_management/pebble_process_md.h>

// PlatformFeature counts the bits of PebblePlatformFeature.
#define ASSERT_SAME_BIT(feature, bit) \
  static_assert(PBL_BIT(feature) == (bit), "PlatformFeature and PebblePlatformFeature differ")
ASSERT_SAME_BIT(PlatformFeatureMicrophone, PLATFORM_FEATURE_MICROPHONE);
ASSERT_SAME_BIT(PlatformFeatureSmartstrap, PLATFORM_FEATURE_SMARTSTRAP);
ASSERT_SAME_BIT(PlatformFeatureSmartstrapPower, PLATFORM_FEATURE_SMARTSTRAP_POWER);
ASSERT_SAME_BIT(PlatformFeatureHealth, PLATFORM_FEATURE_HEALTH);
ASSERT_SAME_BIT(PlatformFeatureCompass, PLATFORM_FEATURE_COMPASS);
ASSERT_SAME_BIT(PlatformFeatureTouch, PLATFORM_FEATURE_TOUCH);
ASSERT_SAME_BIT(PlatformFeatureRGBBacklight, PLATFORM_FEATURE_RGB_BACKLIGHT);
ASSERT_SAME_BIT(PlatformFeatureSpeaker, PLATFORM_FEATURE_SPEAKER);

uint32_t platform_get_features(void) {
  uint32_t features = 0;
#ifdef CONFIG_PLATFORM_MICROPHONE
  features |= PLATFORM_FEATURE_MICROPHONE;
#endif
#ifdef CONFIG_PLATFORM_SMARTSTRAP
  features |= PLATFORM_FEATURE_SMARTSTRAP;
#endif
#ifdef CONFIG_PLATFORM_SMARTSTRAP_POWER
  features |= PLATFORM_FEATURE_SMARTSTRAP_POWER;
#endif
#ifdef CONFIG_PLATFORM_HEALTH
  features |= PLATFORM_FEATURE_HEALTH;
#endif
#ifdef CONFIG_PLATFORM_COMPASS
  features |= PLATFORM_FEATURE_COMPASS;
#endif
#ifdef CONFIG_PLATFORM_TOUCH
  features |= PLATFORM_FEATURE_TOUCH;
#endif
#ifdef CONFIG_PLATFORM_RGB_BACKLIGHT
  features |= PLATFORM_FEATURE_RGB_BACKLIGHT;
#endif
#ifdef CONFIG_PLATFORM_SPEAKER
  features |= PLATFORM_FEATURE_SPEAKER;
#endif
  return features;
}

bool platform_has_feature(PlatformFeature feature) {
  // A newer SDK may know features this firmware does not.
  if ((unsigned int)feature >= 32) {
    return false;
  }
  return (platform_get_features() & PBL_BIT(feature)) != 0;
}
