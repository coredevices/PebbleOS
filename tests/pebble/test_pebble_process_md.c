/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <process_management/pebble_process_md.h>

void test_pebble_process_md__uninitialized(void) {
  PebbleProcessMdFlash md = {};
  const PlatformType type = process_metadata_get_app_sdk_platform(&md.common);
  cl_assert_equal_i(type, PBL_PLATFORM_TYPE_CURRENT);
}

void test_pebble_process_md__uninitialized_unprivileged(void) {
  PebbleProcessMdFlash md = {
    .common.is_unprivileged = true,
  };
  const PlatformType type = process_metadata_get_app_sdk_platform(&md.common);
  cl_assert_equal_i(type, PBL_PLATFORM_TYPE_CURRENT);
}

#if PBL_ROUND
#define LEGACY_PLATFORM_PRIOR_4 PlatformTypeChalk
#define LEGACY_PLATFORM_AFTER_4 PlatformTypeChalk
#elif PBL_RECT
#if PBL_BW
#define LEGACY_PLATFORM_PRIOR_4 PlatformTypeAplite
#define LEGACY_PLATFORM_AFTER_4 PlatformTypeDiorite
#elif PBL_COLOR
#define LEGACY_PLATFORM_PRIOR_4 PlatformTypeBasalt
#define LEGACY_PLATFORM_AFTER_4 PlatformTypeBasalt
#endif
#endif

void test_pebble_process_md__SDK2(void) {
  PebbleProcessMdFlash md = {
    .common.process_storage = ProcessStorageFlash,
    .common.is_unprivileged = true,
    .common.stored_sdk_platform = 0,
    .sdk_version.major = PROCESS_INFO_FIRST_2X_SDK_VERSION_MAJOR,
    .sdk_version.minor = PROCESS_INFO_FIRST_2X_SDK_VERSION_MINOR,
  };
  const PlatformType type = process_metadata_get_app_sdk_platform(&md.common);
  cl_assert_equal_i(type, PlatformTypeAplite);
}

void test_pebble_process_md__SDK3(void) {
  PebbleProcessMdFlash md = {
    .common.process_storage = ProcessStorageFlash,
    .common.is_unprivileged = true,
    .common.stored_sdk_platform = 0,
    .sdk_version.major = PROCESS_INFO_FIRST_3X_SDK_VERSION_MAJOR,
    .sdk_version.minor = PROCESS_INFO_FIRST_3X_SDK_VERSION_MINOR,
  };
  const PlatformType type = process_metadata_get_app_sdk_platform(&md.common);
  cl_assert_equal_i(type, LEGACY_PLATFORM_PRIOR_4);
}

void test_pebble_process_md__SDK4(void) {
  PebbleProcessMdFlash md = {
    .common.process_storage = ProcessStorageFlash,
    .common.is_unprivileged = true,
    .common.stored_sdk_platform = 0,
    .sdk_version.major = PROCESS_INFO_FIRST_4X_SDK_VERSION_MAJOR,
    .sdk_version.minor = PROCESS_INFO_FIRST_4X_SDK_VERSION_MINOR,
  };
  const PlatformType type = process_metadata_get_app_sdk_platform(&md.common);

  cl_assert_equal_i(type, LEGACY_PLATFORM_AFTER_4);
}

void test_pebble_process_md__SDK4_stored_but_ignored(void) {
  // stored platform will be ignored unless SDK version is >= 4.2
  PebbleProcessMdFlash md = {
    .common.process_storage = ProcessStorageFlash,
    .common.is_unprivileged = true,
    .common.stored_sdk_platform = PROCESS_INFO_PLATFORM_CHALK,
    .sdk_version.major = PROCESS_INFO_FIRST_4X_SDK_VERSION_MAJOR,
    .sdk_version.minor = PROCESS_INFO_FIRST_4X_SDK_VERSION_MINOR,
  };
  const PlatformType type = process_metadata_get_app_sdk_platform(&md.common);
  cl_assert_equal_i(type, LEGACY_PLATFORM_AFTER_4);
}

void test_pebble_process_md__SDK4_2(void) {
  PebbleProcessMdFlash md = {
    .common.process_storage = ProcessStorageFlash,
    .common.is_unprivileged = true,
    .common.stored_sdk_platform = PROCESS_INFO_PLATFORM_BASALT,
    .sdk_version.major = PROCESS_INFO_FIRST_4_2_X_SDK_VERSION_MAJOR,
    .sdk_version.minor = PROCESS_INFO_FIRST_4_2_X_SDK_VERSION_MINOR,
  };
  const PlatformType type = process_metadata_get_app_sdk_platform(&md.common);
  cl_assert_equal_i(type, PlatformTypeBasalt);
}

// Framebuffer size
////////////////////////////////////

static const GSize s_display_size = {PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT};

static void prv_assert_framebuffer_size(const PebbleProcessMdFlash *md, GSize expected) {
  const GSize size = process_metadata_get_app_framebuffer_size(&md->common);
  cl_assert_equal_i(size.w, expected.w);
  cl_assert_equal_i(size.h, expected.h);
}

static PebbleProcessMdFlash prv_md_for_platform(uint32_t stored_sdk_platform) {
  return (PebbleProcessMdFlash){
    .common.process_storage = ProcessStorageFlash,
    .common.is_unprivileged = true,
    .common.stored_sdk_platform = stored_sdk_platform,
    .sdk_version.major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR,
    .sdk_version.minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR,
  };
}

static PebbleProcessMdFlash prv_md_for_display(int16_t width, int16_t height,
                                               uint8_t display_shape) {
  PebbleProcessMdFlash md = prv_md_for_platform(PROCESS_INFO_PLATFORM_OTHER);
  md.platform_info = (PebblePlatformInfo){
    .version = PROCESS_PLATFORM_INFO_VERSION,
    .length = sizeof(PebblePlatformInfo),
    .abi = PLATFORM_ABI_ARMV7M,
    .display_shape = display_shape,
    .display_width = width,
    .display_height = height,
  };
  return md;
}

#define OUR_SHAPE   PBL_IF_ROUND_ELSE(PLATFORM_DISPLAY_SHAPE_ROUND, PLATFORM_DISPLAY_SHAPE_RECT)
#define OTHER_SHAPE PBL_IF_ROUND_ELSE(PLATFORM_DISPLAY_SHAPE_RECT, PLATFORM_DISPLAY_SHAPE_ROUND)

void test_pebble_process_md__framebuffer_of_system_app(void) {
  const PebbleProcessMdFlash md = {};
  prv_assert_framebuffer_size(&md, s_display_size);
}

void test_pebble_process_md__framebuffer_of_app_for_our_display(void) {
  const PebbleProcessMdFlash md =
      prv_md_for_display(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT, OUR_SHAPE);
  prv_assert_framebuffer_size(&md, s_display_size);
}

void test_pebble_process_md__framebuffer_of_app_for_smaller_display(void) {
#if PBL_RECT
  PebbleProcessMdFlash md = prv_md_for_display(120, 140, PLATFORM_DISPLAY_SHAPE_RECT);
  prv_assert_framebuffer_size(&md, GSize(120, 140));

  // Smaller in one direction only
  md = prv_md_for_display(PBL_DISPLAY_WIDTH, 100, PLATFORM_DISPLAY_SHAPE_RECT);
  prv_assert_framebuffer_size(&md, GSize(PBL_DISPLAY_WIDTH, 100));
#else
  // Round framebuffers only come in the sizes that have a row table
  PebbleProcessMdFlash md = prv_md_for_display(180, 180, PLATFORM_DISPLAY_SHAPE_ROUND);
  prv_assert_framebuffer_size(&md, GSize(180, 180));

  md = prv_md_for_display(200, 200, PLATFORM_DISPLAY_SHAPE_ROUND);
  prv_assert_framebuffer_size(&md, s_display_size);
#endif
}

void test_pebble_process_md__framebuffer_of_app_that_does_not_fit(void) {
  PebbleProcessMdFlash md =
      prv_md_for_display(PBL_DISPLAY_WIDTH + 1, PBL_DISPLAY_HEIGHT, OUR_SHAPE);
  prv_assert_framebuffer_size(&md, s_display_size);

  md = prv_md_for_display(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT + 1, OUR_SHAPE);
  prv_assert_framebuffer_size(&md, s_display_size);

  // Shapes never mix
  md = prv_md_for_display(100, 100, OTHER_SHAPE);
  prv_assert_framebuffer_size(&md, s_display_size);

  // Nor does a shape we do not know fit
  md = prv_md_for_display(100, 100, 2);
  prv_assert_framebuffer_size(&md, s_display_size);
}

void test_pebble_process_md__framebuffer_of_flexible_app(void) {
  // It has no display size of its own and draws to ours
  PebbleProcessMdFlash md = prv_md_for_display(0, 0, OUR_SHAPE);
  md.platform_info.flags = PROCESS_PLATFORM_INFO_FLAG_FLEXIBLE;
  prv_assert_framebuffer_size(&md, s_display_size);
}

void test_pebble_process_md__required_features(void) {
  const uint32_t watch = PLATFORM_FEATURE_MICROPHONE | PLATFORM_FEATURE_HEALTH;

  // A flexible app needs every feature it lists
  PebbleProcessMdFlash md = prv_md_for_display(0, 0, OUR_SHAPE);
  md.platform_info.flags = PROCESS_PLATFORM_INFO_FLAG_FLEXIBLE;
  cl_assert(process_metadata_has_required_features(&md.common, watch));
  md.platform_info.features = PLATFORM_FEATURE_MICROPHONE;
  cl_assert(process_metadata_has_required_features(&md.common, watch));
  md.platform_info.features = PLATFORM_FEATURE_MICROPHONE | PLATFORM_FEATURE_TOUCH;
  cl_assert(!process_metadata_has_required_features(&md.common, watch));
  // One nobody has defined yet is one no watch has
  md.platform_info.features = 1u << 31;
  cl_assert(!process_metadata_has_required_features(&md.common, watch));

  // An app built for a platform only says what that platform has
  md = prv_md_for_display(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT, OUR_SHAPE);
  md.platform_info.features = PLATFORM_FEATURE_TOUCH;
  cl_assert(process_metadata_has_required_features(&md.common, watch));

  // And one without a record says nothing
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_BASALT);
  cl_assert(process_metadata_has_required_features(&md.common, 0));
}

void test_pebble_process_md__framebuffer_of_app_without_platform_info(void) {
  // Legacy platforms, as before
  PebbleProcessMdFlash md = prv_md_for_platform(PROCESS_INFO_PLATFORM_APLITE);
  prv_assert_framebuffer_size(&md, GSize(LEGACY_2X_DISP_COLS, LEGACY_2X_DISP_ROWS));
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_BASALT);
  prv_assert_framebuffer_size(&md, GSize(LEGACY_3X_DISP_COLS, LEGACY_3X_DISP_ROWS));
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_CHALK);
  prv_assert_framebuffer_size(&md, GSize(LEGACY_3X_DISP_COLS, LEGACY_3X_DISP_ROWS));

  // The others by the display of their platform, when it fits ours
  const GSize diorite_size = PBL_IF_ROUND_ELSE(s_display_size, GSize(144, 168));
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_DIORITE);
  prv_assert_framebuffer_size(&md, diorite_size);
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_FLINT);
  prv_assert_framebuffer_size(&md, diorite_size);
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_EMERY);
  prv_assert_framebuffer_size(&md, s_display_size);
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_GABBRO);
  prv_assert_framebuffer_size(&md, s_display_size);

  // A platform without a value and without a record is taken to be ours
  md = prv_md_for_platform(PROCESS_INFO_PLATFORM_OTHER);
  prv_assert_framebuffer_size(&md, s_display_size);
}
