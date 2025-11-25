/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/app_glance.h"
#include "drivers/rtc.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_install_manager.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/app_glances/app_glance_service.h"
#include "services/normal/blob_db/app_glance_db.h"
#include "services/normal/blob_db/app_glance_db_private.h"
#include "services/normal/filesystem/pfs.h"
#include "util/uuid.h"

// Fakes
////////////////////////////////////////////////////////////////

#include "fake_settings_file.h"

// Stubs
////////////////////////////////////////////////////////////////

#include "stubs_app_cache.h"
#include "stubs_app_install_manager.h"
#include "stubs_events.h"
#include "stubs_event_service_client.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

status_t pfs_remove(const char *name) {
  fake_settings_file_reset();
  return S_SUCCESS;
}

#define APP_GLANCE_TEST_UUID \
    (UuidMake(0x3d, 0xc6, 0xb9, 0x4c, 0x4, 0x2, 0x48, 0xf4, \
              0xbe, 0x14, 0x81, 0x17, 0xf1, 0xa, 0xa9, 0xc4))

// Setup
////////////////////////////////////////////////////////////////

void test_app_glance_service__initialize(void) {
  fake_settings_file_reset();
  app_glance_db_init();
  app_glance_service_init();
}

void app_glance_db_deinit(void);

void test_app_glance_service__cleanup(void) {
  app_glance_db_deinit();
}

static void prv_check_expected_slice_data(const AppGlanceSliceInternal *expected_slice_data,
                                          time_t time) {
  // Set the current time to the provided time
  rtc_set_time(time);

  AppGlanceSliceInternal slice_out;
  if (expected_slice_data) {
    // Request the current slice from the glance, expecting it to return true
    cl_assert_equal_b(app_glance_service_get_current_slice(&APP_GLANCE_TEST_UUID, &slice_out),
                      true);
    // Check that it matches the slice data we expect
    cl_assert_equal_m(&slice_out, expected_slice_data, sizeof(AppGlanceSliceInternal));
  } else {
    // Check that requesting the current slice returns false
    cl_assert_equal_b(app_glance_service_get_current_slice(&APP_GLANCE_TEST_UUID, &slice_out),
                      false);
  }
}

void test_app_glance_service__get_current_slice_basic(void) {
  AppGlanceSliceInternal slice_out;

  // Requesting the current slice with a NULL UUID should return false
  cl_assert_equal_b(app_glance_service_get_current_slice(NULL, &slice_out), false);

  // Requesting the current slice with an invalid UUID should return false
  cl_assert_equal_b(app_glance_service_get_current_slice(&UUID_INVALID, &slice_out), false);

  // Requesting the current slice with a NULL slice_out argument should return false
  cl_assert_equal_b(app_glance_service_get_current_slice(&APP_GLANCE_TEST_UUID, NULL), false);

  // Insert a glance
  const AppGlance glance = (AppGlance) {
    .num_slices = 1,
    .slices = {
      {
        .expiration_time = 1464734484, // (Tue, 31 May 2016 22:41:24 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .icon_resource_id = RESOURCE_ID_SETTINGS_ICON_AIRPLANE,
          .template_string = "Test subtitle",
        },
      },
    },
  };
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID, &glance), S_SUCCESS);

  // This is the slice we expect to get when we request the current slice
  // Note that we don't expect the pointer to the slice but rather the data behind the pointer
  const AppGlanceSliceInternal *expected_slice_data = &glance.slices[0];

  // Since there's only one slice in the glance, check that we get it 100 seconds before its
  // expiration time
  prv_check_expected_slice_data(expected_slice_data, expected_slice_data->expiration_time - 100);

  // Check that we get back nothing for the current slice if we set the time to exactly when the
  // only slice in the glance expires
  prv_check_expected_slice_data(NULL, expected_slice_data->expiration_time);
}

void test_app_glance_service__get_current_slice_from_glance_with_multiple_unsorted_slices(void) {
  // Insert a glance with multiple, unsorted slices
  // Note that the expiration time for each of these are a minimum of 10 seconds apart
  const AppGlance glance = (AppGlance) {
    .num_slices = 2,
    .slices = {
      {
        .expiration_time = 1464734504, // (Tue, 31 May 2016 22:41:44 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .icon_resource_id = RESOURCE_ID_SETTINGS_ICON_BLUETOOTH_ALT,
          .template_string = "Test subtitle 2",
        },
      },
      {
        .expiration_time = 1464734484, // (Tue, 31 May 2016 22:41:24 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .icon_resource_id = RESOURCE_ID_SETTINGS_ICON_AIRPLANE,
          .template_string = "Test subtitle 1",
        },
      },
    },
  };
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID, &glance), S_SUCCESS);

  // This is the slice we expect to get when we request the current slice
  // Note that we don't expect the pointer to the slice but rather the data behind the pointer
  const AppGlanceSliceInternal *expected_slice_data = &glance.slices[1];

  // Set the current time to 5 seconds before the slice we expect to get back expires and check
  // that we get it
  prv_check_expected_slice_data(expected_slice_data, expected_slice_data->expiration_time - 5);

  // Try again for the next slice that expires
  expected_slice_data = &glance.slices[0];
  prv_check_expected_slice_data(expected_slice_data, expected_slice_data->expiration_time - 5);

  // Finally, check that after all slices have expired we get back nothing for the current slice
  prv_check_expected_slice_data(NULL, expected_slice_data->expiration_time);
}

void test_app_glance_service__slice_with_no_expiration(void) {
  // Insert a glance with multiple, unsorted slices
  // Note that the expiration time for each of these are a minimum of 10 seconds apart
  const AppGlance glance = (AppGlance) {
    .num_slices = 2,
    .slices = {
      {
        .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .icon_resource_id = RESOURCE_ID_SETTINGS_ICON_AIRPLANE,
          .template_string = "Test subtitle 2",
        },
      },
      {
      .expiration_time = 1464734504, // (Tue, 31 May 2016 22:41:44 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .icon_resource_id = RESOURCE_ID_SETTINGS_ICON_BLUETOOTH_ALT,
          .template_string = "Test subtitle 1",
        },
      },
    },
  };
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID, &glance), S_SUCCESS);

  // We expect the slice with the defined expiration time when we request the current slice
  // Note that we don't expect the pointer to the slice but rather the data behind the pointer
  const AppGlanceSliceInternal *expiring_slice_data = &glance.slices[1];

  // Set the current time to 5 seconds before the slice we expect to get back expires and check
  // that we get it
  prv_check_expected_slice_data(expiring_slice_data, expiring_slice_data->expiration_time - 5);

  // Check that we get the slice that never expires 5 seconds after the expiring slice expires
  const AppGlanceSliceInternal *no_expire_slice_data = &glance.slices[0];
  prv_check_expected_slice_data(no_expire_slice_data, expiring_slice_data->expiration_time + 5);

  // Check that going super far into the future still returns the slice with no expiration time
  prv_check_expected_slice_data(no_expire_slice_data,
                                expiring_slice_data->expiration_time + 9999999);
}
