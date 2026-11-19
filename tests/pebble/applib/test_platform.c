/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/platform.h>
#include <clar.h>
#include <process_management/pebble_process_md.h>

void test_platform__features_every_platform_has(void) {
  cl_assert(platform_has_feature(PlatformFeatureMicrophone));
  cl_assert(platform_has_feature(PlatformFeatureHealth));
  cl_assert(platform_has_feature(PlatformFeatureCompass));
}

void test_platform__features_some_platforms_have(void) {
#if defined(CONFIG_PLATFORM_EMERY)
  cl_assert(platform_has_feature(PlatformFeatureSmartstrap));
  cl_assert(platform_has_feature(PlatformFeatureSmartstrapPower));
  cl_assert(platform_has_feature(PlatformFeatureTouch));
  cl_assert(platform_has_feature(PlatformFeatureRGBBacklight));
  cl_assert(platform_has_feature(PlatformFeatureSpeaker));
#elif defined(CONFIG_PLATFORM_FLINT)
  cl_assert(!platform_has_feature(PlatformFeatureSmartstrap));
  cl_assert(!platform_has_feature(PlatformFeatureSmartstrapPower));
  cl_assert(!platform_has_feature(PlatformFeatureTouch));
  cl_assert(!platform_has_feature(PlatformFeatureRGBBacklight));
  cl_assert(platform_has_feature(PlatformFeatureSpeaker));
#elif defined(CONFIG_PLATFORM_GABBRO)
  cl_assert(!platform_has_feature(PlatformFeatureSmartstrap));
  cl_assert(platform_has_feature(PlatformFeatureTouch));
  cl_assert(!platform_has_feature(PlatformFeatureSpeaker));
#else
#error "Not a platform this test knows"
#endif
}

void test_platform__features_are_those_the_watch_reports(void) {
  uint32_t features = 0;
  for (int feature = 0; feature < 32; feature++) {
    if (platform_has_feature((PlatformFeature)feature)) {
      features |= PBL_BIT(feature);
    }
  }
  cl_assert_equal_i(features, platform_get_features());
}

void test_platform__unknown_feature(void) {
  // One an app built with a newer SDK may ask for.
  cl_assert(!platform_has_feature((PlatformFeature)31));
  cl_assert(!platform_has_feature((PlatformFeature)32));
  cl_assert(!platform_has_feature((PlatformFeature)-1));
}
