/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "process_management/pebble_process_md.h"

void test_pebble_process_md__uninitialized(void) {
  PebbleProcessMdFlash md = {
  };
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
