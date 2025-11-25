/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "process_management/app_install_manager.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/timeline_resources.h"
#include "system/passert.h"
#include "util/struct.h"

// Stubs
/////////////////////////

#include "stubs_kino_reel.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"

// Test Data
/////////////////////////

typedef enum TimelineResourceTestAppTimelineId {
  // We start at 1 because TIMELINE_RESOURCE_INVALID = 0
  TimelineResourceTestTimelineId_AlarmClock = 1,
  TimelineResourceTestTimelineId_Basketball,

  TimelineResourceTestTimelineIdCount
} TimelineResourceTestTimelineId;

static const uint32_t s_app_lut[TimelineResourceTestTimelineIdCount][TimelineResourceSizeCount] = {
  [TIMELINE_RESOURCE_INVALID] = {
    RESOURCE_ID_INVALID, RESOURCE_ID_INVALID, RESOURCE_ID_INVALID
  },
  [TimelineResourceTestTimelineId_AlarmClock] = {
    RESOURCE_ID_ALARM_CLOCK_TINY, RESOURCE_ID_ALARM_CLOCK_SMALL, RESOURCE_ID_ALARM_CLOCK_LARGE
  },
  [TimelineResourceTestTimelineId_Basketball] = {
    RESOURCE_ID_BASKETBALL_TINY, RESOURCE_ID_BASKETBALL_SMALL, RESOURCE_ID_BASKETBALL_LARGE
  },
};

typedef struct TimelineResourceTestAppData {
  AppInstallEntry install_entry;
  const uint32_t (*resource_lut)[TimelineResourceSizeCount];
} TimelineResourceTestAppData;

typedef enum TimelineResourceTestAppId {
  // We start from 1 because INSTALL_ID_INVALID = 0
  TimelineResourceTestAppId_AppWithInvalidLUT = 1,
  TimelineResourceTestAppId_AppWithInvalidSDKVersion,
  TimelineResourceTestAppId_ValidApp,

  TimelineResourceTestAppIdInvalid,
  TimelineResourceTestAppIdCount = TimelineResourceTestAppIdInvalid - 1
} TimelineResourceTestAppId;

static const TimelineResourceTestAppData s_test_apps[TimelineResourceTestAppIdCount] = {
  {
    .install_entry = {
      .install_id = TimelineResourceTestAppId_AppWithInvalidLUT,
      .uuid = {0x3c, 0x6e, 0x2e, 0x1d, 0x61, 0x7d, 0x4d, 0x17,
               0x97, 0xa1, 0xbc, 0x43, 0x2d, 0x87, 0x4c, 0xed},
      .sdk_version = {TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MAJOR,
                      TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MINOR},
    },
    // No resource_lut specified because this app has an "invalid" lut
  },
  {
    .install_entry = {
      .install_id = TimelineResourceTestAppId_AppWithInvalidSDKVersion,
      .uuid = {0x37, 0xe7, 0x64, 0x5e, 0xd, 0x6a, 0x41, 0xfe,
               0xb8, 0x80, 0xea, 0x47, 0x5a, 0x5f, 0x34, 0x34},
      // We set the SDK version to one earlier than the first version supporting timeline resources
      .sdk_version = {TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MAJOR,
                      TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MINOR - 1},
    },
    .resource_lut = s_app_lut,
  },
  {
    .install_entry = {
      .install_id = TimelineResourceTestAppId_ValidApp,
      .uuid = {0x9e, 0x95, 0x8b, 0xfe, 0xd, 0xbd, 0x4d, 0xf2,
               0xbe, 0xbc, 0xf3, 0x77, 0x5d, 0x8d, 0x9f, 0x95},
      .sdk_version = {TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MAJOR,
                      TIMELINE_RESOURCE_PBW_SUPPORT_FIRST_SDK_VERSION_MINOR},
    },
    .resource_lut = s_app_lut,
  },
};

static const TimelineResourceTestAppData *prv_get_data_for_app_with_id(AppInstallId install_id) {
  for (int i = 0; i < TimelineResourceTestAppIdCount; i++) {
    const TimelineResourceTestAppData *data = &s_test_apps[i];
    if (data->install_entry.install_id == install_id) {
      return data;
    }
  }
  return NULL;
}

static const TimelineResourceTestAppData *prv_get_data_for_app_with_uuid(const Uuid *uuid) {
  for (int i = 0; i < TimelineResourceTestAppIdCount; i++) {
    const TimelineResourceTestAppData *data = &s_test_apps[i];
    if (uuid_equal(uuid, &data->install_entry.uuid)) {
      return data;
    }
  }
  return NULL;
}

// Fakes
/////////////////////////

bool prv_validate_lut(ResAppNum res_app_num) {
  // Just check if the .resource_lut pointer for the provided res_app_num is non-NULL
  const TimelineResourceTestAppData *data = prv_get_data_for_app_with_id(res_app_num);
  return data ? (data->resource_lut != NULL) : false;
}

uint32_t prv_get_app_resource_id(ResAppNum res_app_num, TimelineResourceId timeline_id,
                                 TimelineResourceSize size) {
  // Size must be valid
  if (size >= TimelineResourceSizeCount) {
    return RESOURCE_ID_INVALID;
  }

  // This only supports valid non-system apps
  const TimelineResourceTestAppData *data = prv_get_data_for_app_with_id(res_app_num);
  if (!data) {
    return RESOURCE_ID_INVALID;
  }

  // The app must have a valid LUT
  if (!data->resource_lut) {
    return RESOURCE_ID_INVALID;
  }

  return data->resource_lut[timeline_id][size];
}

static bool s_is_app_published_resource_invalid;

bool prv_is_app_published_resource_valid(const AppResourceInfo *res_info) {
  return !s_is_app_published_resource_invalid;
}

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  const TimelineResourceTestAppData *data = prv_get_data_for_app_with_uuid(uuid);
  return NULL_SAFE_FIELD_ACCESS(data, install_entry.install_id, INSTALL_ID_INVALID);
}

bool app_install_get_entry_for_install_id(AppInstallId install_id, AppInstallEntry *entry) {
  if (!entry) {
    return false;
  }

  const TimelineResourceTestAppData *data = prv_get_data_for_app_with_id(install_id);
  if (!data) {
    return false;
  }
  *entry = data->install_entry;
  return true;
}

ResAppNum app_install_get_app_icon_bank(const AppInstallEntry *entry) {
  PBL_ASSERTN(entry);
  if (uuid_equal(&entry->uuid, &(Uuid)UUID_SYSTEM)) {
    return SYSTEM_APP;
  } else {
    return entry->install_id;
  }
}

// Setup
/////////////////////////

void test_timeline_resources__initialize(void) {
  s_is_app_published_resource_invalid = false;
}

// Tests
/////////////////////////

void test_timeline_resources__get_id_system(void) {
  AppResourceInfo res_info;

  // Calling the function with an invalid TimelineResourceId should return false
  cl_assert(!timeline_resources_get_id_system(TIMELINE_RESOURCE_INVALID, TimelineResourceSizeTiny,
            TimelineResourceTestAppId_ValidApp, &res_info));

  // Calling the function with an invalid size should return false
  cl_assert(!timeline_resources_get_id_system(
      (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock, TimelineResourceSizeCount,
      TimelineResourceTestAppId_ValidApp, &res_info));

  // Calling the function with the ResAppNum of an app with an invalid LUT should return false
  cl_assert(!timeline_resources_get_id_system(
      (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock, TimelineResourceSizeTiny,
      TimelineResourceTestAppId_AppWithInvalidLUT, &res_info));

  // Calling the function for an invalid resource (e.g. dimensions too large) should return false
  s_is_app_published_resource_invalid = true;
  cl_assert(!timeline_resources_get_id_system(
      (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock, TimelineResourceSizeTiny,
      TimelineResourceTestAppId_ValidApp, &res_info));
  s_is_app_published_resource_invalid = false;

  // Calling the function with valid args should return true and set the correct values in res_info
  cl_assert(timeline_resources_get_id_system(
      (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock, TimelineResourceSizeTiny,
      TimelineResourceTestAppId_ValidApp, &res_info));
  cl_assert_equal_i(res_info.res_app_num, TimelineResourceTestAppId_ValidApp);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_ALARM_CLOCK_TINY);

  // Calling the function with valid args should return true even if no AppResourceInfo is provided
  cl_assert(timeline_resources_get_id_system(
      (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock, TimelineResourceSizeTiny,
      TimelineResourceTestAppId_ValidApp, NULL));

  // Calling the function with a valid system TimelineResourceId should return true and set res_info
  cl_assert(timeline_resources_get_id_system(TIMELINE_RESOURCE_AUDIO_CASSETTE,
                                             TimelineResourceSizeSmall, SYSTEM_APP, &res_info));
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_AUDIO_CASSETTE_SMALL);
  // Even if the provided ResAppNum != SYSTEM_APP
  cl_assert(timeline_resources_get_id_system(TIMELINE_RESOURCE_AUDIO_CASSETTE,
                                             TimelineResourceSizeSmall, TIMELINE_RESOURCE_INVALID,
                                             &res_info));
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_AUDIO_CASSETTE_SMALL);
}

void test_timeline_resources__get_id(void) {
  TimelineResourceInfo timeline_res_info;
  AppResourceInfo res_info;

  const TimelineResourceTestAppData *valid_app_data =
      prv_get_data_for_app_with_id(TimelineResourceTestAppId_ValidApp);
  PBL_ASSERTN(valid_app_data);

  // Calling the function with an invalid TimelineResourceId should set res_info to the fallback
  timeline_res_info = (TimelineResourceInfo) {
    .app_id = &valid_app_data->install_entry.uuid,
    .res_id = TIMELINE_RESOURCE_INVALID,
    .fallback_id = TIMELINE_RESOURCE_BIRTHDAY_EVENT,
  };
  timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeLarge, &res_info);
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_BIRTHDAY_EVENT_LARGE);

  // Set the TimelineResourceInfo to valid values
  timeline_res_info = (TimelineResourceInfo) {
    .app_id = &valid_app_data->install_entry.uuid,
    .res_id = (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock,
    .fallback_id = TIMELINE_RESOURCE_BIRTHDAY_EVENT,
  };

  // Calling the function with an invalid size, no TimelineResourceInfo, or no AppResourceInfo
  // should assert
  cl_assert_passert(timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeCount,
                                              &res_info));
  cl_assert_passert(timeline_resources_get_id(NULL, TimelineResourceSizeTiny, &res_info));
  cl_assert_passert(timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeTiny, NULL));

  // Set the TimelineResourceInfo to have the UUID of an app with an invalid LUT
  const TimelineResourceTestAppData *app_with_invalid_lut_data =
      prv_get_data_for_app_with_id(TimelineResourceTestAppId_AppWithInvalidLUT);
  PBL_ASSERTN(app_with_invalid_lut_data);
  timeline_res_info = (TimelineResourceInfo) {
    .app_id = &app_with_invalid_lut_data->install_entry.uuid,
    .res_id = (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock,
    .fallback_id = TIMELINE_RESOURCE_BIRTHDAY_EVENT,
  };

  // Calling the function with the UUID of an app with an invalid LUT should set res_info to the
  // fallback
  timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeLarge, &res_info);
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_BIRTHDAY_EVENT_LARGE);

  // Set the TimelineResourceInfo to valid values
  timeline_res_info = (TimelineResourceInfo) {
    .app_id = &valid_app_data->install_entry.uuid,
    .res_id = (TimelineResourceId)TimelineResourceTestTimelineId_AlarmClock,
    .fallback_id = TIMELINE_RESOURCE_BIRTHDAY_EVENT,
  };

  // Calling the function for an invalid resource (e.g. dimensions too large) should set res_info
  // to the fallback
  s_is_app_published_resource_invalid = true;
  timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeLarge, &res_info);
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_BIRTHDAY_EVENT_LARGE);
  s_is_app_published_resource_invalid = false;

  // Calling the function with valid args should return true and set the correct values in res_info
  timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeLarge, &res_info);
  cl_assert_equal_i(res_info.res_app_num, TimelineResourceTestAppId_ValidApp);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_ALARM_CLOCK_LARGE);

  // Set the TimelineResourceInfo to have the UUID of an app with an unsupported SDK version
  const TimelineResourceTestAppData *app_with_invalid_sdk_version =
      prv_get_data_for_app_with_id(TimelineResourceTestAppId_AppWithInvalidSDKVersion);
  PBL_ASSERTN(app_with_invalid_sdk_version);
  timeline_res_info = (TimelineResourceInfo) {
    .app_id = &app_with_invalid_sdk_version->install_entry.uuid,
    .res_id = (TimelineResourceId)TimelineResourceTestTimelineId_Basketball,
    .fallback_id = TIMELINE_RESOURCE_BIRTHDAY_EVENT,
  };

  // Calling the function with the UUID of an app with an unsupported SDK version should set
  // res_info to the fallback
  timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeTiny, &res_info);
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_BIRTHDAY_EVENT_TINY);

  // Set the TimelineResourceInfo to valid values but with a system TimelineResourceId requested
  timeline_res_info = (TimelineResourceInfo) {
    .app_id = &(Uuid)UUID_SYSTEM,
    .res_id = TIMELINE_RESOURCE_HOTEL_RESERVATION,
    .fallback_id = TIMELINE_RESOURCE_BIRTHDAY_EVENT,
  };

  // Calling the function with a valid system TimelineResourceId should set the correct values in
  // res_info
  timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeSmall, &res_info);
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_HOTEL_RESERVATION_SMALL);
  // Even if the provided app UUID != UUID_SYSTEM
  timeline_res_info.app_id = &valid_app_data->install_entry.uuid;
  timeline_resources_get_id(&timeline_res_info, TimelineResourceSizeSmall, &res_info);
  cl_assert_equal_i(res_info.res_app_num, SYSTEM_APP);
  cl_assert_equal_i(res_info.res_id, RESOURCE_ID_HOTEL_RESERVATION_SMALL);
}

void test_timeline_resources__is_system(void) {
  // System TimelineResourceIds should return true
  cl_assert(timeline_resources_is_system(TIMELINE_RESOURCE_AUDIO_CASSETTE));

  // Others should return false
  cl_assert(!timeline_resources_is_system(TIMELINE_RESOURCE_INVALID));
  cl_assert(!timeline_resources_is_system(NUM_TIMELINE_RESOURCES));
}
