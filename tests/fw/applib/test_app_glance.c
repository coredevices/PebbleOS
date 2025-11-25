/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/app_glance.h"
#include "drivers/rtc.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_install_manager.h"
#include "resource/resource_ids.auto.h"
#include "resource/timeline_resource_ids.auto.h"
#include "services/normal/app_glances/app_glance_service.h"
#include "services/normal/blob_db/app_glance_db.h"
#include "services/normal/blob_db/app_glance_db_private.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/timeline/timeline_resources.h"
#include "util/uuid.h"

#define APP_GLANCE_TEST_UUID \
    (UuidMake(0x3d, 0xc6, 0xb9, 0x4c, 0x4, 0x2, 0x48, 0xf4, \
              0xbe, 0x14, 0x81, 0x17, 0xf1, 0xa, 0xa9, 0xc4))

// Fakes
////////////////////////////////////////////////////////////////

#include "fake_rtc.h"
#include "fake_settings_file.h"

void sys_get_app_uuid(Uuid *uuid) {
  if (uuid) {
    *uuid = APP_GLANCE_TEST_UUID;
  }
}

typedef struct AppGlanceTestState {
  bool resource_is_valid;
  void *context;
  bool reload_callback_was_called;
} AppGlanceTestState;

static AppGlanceTestState s_test_state;

ResAppNum sys_get_current_resource_num(void) {
  return 0;
}

void sys_timeline_resources_get_id(const TimelineResourceInfo *timeline_res,
                                   TimelineResourceSize size, AppResourceInfo *res_info) {
  if (!res_info) {
    return;
  }
  // Just fill the output resource ID with some number so it's considered "valid"
  res_info->res_id = s_test_state.resource_is_valid ? 1337 : 0;
}


// Stubs
////////////////////////////////////////////////////////////////

#include "stubs_app_cache.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_events.h"
#include "stubs_event_service_client.h"
#include "stubs_i18n.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

status_t pfs_remove(const char *name) {
  fake_settings_file_reset();
  return S_SUCCESS;
}

// Setup
////////////////////////////////////////////////////////////////

void test_app_glance__initialize(void) {
  fake_rtc_init(0, 1337);
  fake_settings_file_reset();
  app_glance_db_init();
  app_glance_service_init();

  s_test_state = (AppGlanceTestState) {};
}

void app_glance_db_deinit(void);

void test_app_glance__cleanup(void) {
  app_glance_db_deinit();
}

void prv_basic_reload_cb(AppGlanceReloadSession *session, size_t limit, void *context) {
  s_test_state.reload_callback_was_called = true;

  s_test_state.resource_is_valid = true;
  AppGlanceSlice slice = (AppGlanceSlice) {
    .expiration_time = rtc_get_time() + 10,
    .layout.icon = TIMELINE_RESOURCE_HOTEL_RESERVATION,
    .layout.subtitle_template_string = "Test subtitle",
  };
  cl_assert_equal_i(app_glance_add_slice(session, slice), APP_GLANCE_RESULT_SUCCESS);

  slice = (AppGlanceSlice) {
    .expiration_time = rtc_get_time() + 20,
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
  };
  cl_assert_equal_i(app_glance_add_slice(session, slice), APP_GLANCE_RESULT_SUCCESS);
}

void test_app_glance__basic_reload(void) {
  // Reload the glance with two slices
  app_glance_reload(prv_basic_reload_cb, s_test_state.context);
  cl_assert_equal_b(s_test_state.reload_callback_was_called, true);

  // Read the glance back
  AppGlance glance = {};
  cl_assert_equal_i(app_glance_db_read_glance(&APP_GLANCE_TEST_UUID, &glance), S_SUCCESS);

  // Compare the glance read back with the expected glance below
  AppGlance expected_glance = (AppGlance) {
    .num_slices = 2,
    .slices = {
      {
        .expiration_time = rtc_get_time() + 10,
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle.icon_resource_id = TIMELINE_RESOURCE_HOTEL_RESERVATION,
        .icon_and_subtitle.template_string = "Test subtitle",
      },
      {
        .expiration_time = rtc_get_time() + 20,
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle.icon_resource_id = APP_GLANCE_SLICE_DEFAULT_ICON,
      }
    },
  };
  cl_assert_equal_m(&glance, &expected_glance, sizeof(AppGlance));
}

void prv_reload_with_validation_cb(AppGlanceReloadSession *session, size_t limit, void *context) {
  s_test_state.reload_callback_was_called = true;

  // Check that the context here is the context we passed to `app_glance_reload()`
  cl_assert_equal_p(context, s_test_state.context);

  // Check that the limit passed in matches the max slices per glance
  cl_assert_equal_i(limit, APP_GLANCE_DB_MAX_SLICES_PER_GLANCE);

  unsigned int num_slices_added = 0;
  AppGlanceSlice slice = {};

  // Check that using a bogus session variable fails
  AppGlanceReloadSession bogus_session;
  cl_assert(app_glance_add_slice(&bogus_session, slice) & APP_GLANCE_RESULT_INVALID_SESSION);

  // Check that adding a slice with APP_GLANCE_SLICE_DEFAULT_ICON as the icon succeeds
  slice = (AppGlanceSlice) {
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = "Test subtitle {time_until(500)|format('%uS')}",
  };
  cl_assert_equal_i(app_glance_add_slice(session, slice), APP_GLANCE_RESULT_SUCCESS);
  num_slices_added++;

  // Check that adding a slice with a NULL subtitle succeeds
  s_test_state.resource_is_valid = true;
  slice = (AppGlanceSlice) {
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
    .layout.icon = TIMELINE_RESOURCE_BIRTHDAY_EVENT,
    .layout.subtitle_template_string = NULL,
  };
  cl_assert_equal_i(app_glance_add_slice(session, slice), APP_GLANCE_RESULT_SUCCESS);
  num_slices_added++;

  // Check that adding a slice with an invalid icon fails
  s_test_state.resource_is_valid = false;
  slice = (AppGlanceSlice) {
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
    .layout.icon = RESOURCE_ID_SETTINGS_ICON_AIRPLANE,
    .layout.subtitle_template_string = "Test subtitle",
  };
  cl_assert(app_glance_add_slice(session, slice) & APP_GLANCE_RESULT_INVALID_ICON);

  // Check that adding a slice with a subtitle that's too long fails
  const char *really_long_subtitle = "This is a really really really really really really really "
                                     "really really really really really really really really "
                                     "really really really really really really really really "
                                     "really really really really really really really really "
                                     "really long subtitle.";
  slice = (AppGlanceSlice) {
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = really_long_subtitle,
  };
  cl_assert(app_glance_add_slice(session, slice) & APP_GLANCE_RESULT_TEMPLATE_STRING_TOO_LONG);
 
  // Check that adding a slice with a bad template string fails
  const char *invalid_template_subtitle = "How much time? {time_until(500)|format('%uS',)}";
  slice = (AppGlanceSlice) {
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = invalid_template_subtitle,
  };
  cl_assert(app_glance_add_slice(session, slice) & APP_GLANCE_RESULT_INVALID_TEMPLATE_STRING);

  // Check that adding a slice that expires in the past fails
  slice = (AppGlanceSlice) {
    .expiration_time = rtc_get_time() - 10,
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = "Test subtitle",
  };
  cl_assert(app_glance_add_slice(session, slice) & APP_GLANCE_RESULT_EXPIRES_IN_THE_PAST);

  // At this point we've actually filled up the glance to the capacity
  cl_assert_equal_i(num_slices_added, limit);

  // So adding one more slice to the glance should fail
  s_test_state.resource_is_valid = true;
  slice = (AppGlanceSlice) {
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
    .layout.subtitle_template_string = NULL,
  };
  cl_assert(app_glance_add_slice(session, slice) & APP_GLANCE_RESULT_SLICE_CAPACITY_EXCEEDED);

  // Check that we can get reports of multiple kinds of failures at the same time
  s_test_state.resource_is_valid = false;
  slice = (AppGlanceSlice) {
    .expiration_time = rtc_get_time() - 10,
    .layout.icon = RESOURCE_ID_SETTINGS_ICON_AIRPLANE,
    .layout.subtitle_template_string = really_long_subtitle,
  };
  const AppGlanceResult result = app_glance_add_slice(session, slice);
  cl_assert(result & APP_GLANCE_RESULT_EXPIRES_IN_THE_PAST);
  cl_assert(result & APP_GLANCE_RESULT_SLICE_CAPACITY_EXCEEDED);
  cl_assert(result & APP_GLANCE_RESULT_INVALID_ICON);
  cl_assert(result & APP_GLANCE_RESULT_TEMPLATE_STRING_TOO_LONG);
}

void test_app_glance__reload_with_validation_callback(void) {
  app_glance_reload(prv_reload_with_validation_cb, s_test_state.context);
  cl_assert_equal_b(s_test_state.reload_callback_was_called, true);
}

static void prv_glance_clear_test(AppGlanceReloadCallback reload_cb) {
  // Insert some slices for the glance
  const AppGlance glance = (AppGlance) {
    .num_slices = 2,
    .slices = {
      {
        .expiration_time = 1464734504, // (Tue, 31 May 2016 22:41:44 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .template_string = "Test subtitle 2",
        },
      },
      {
        .expiration_time = 1464734484, // (Tue, 31 May 2016 22:41:24 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .template_string = "Test subtitle 1",
        },
      },
    },
  };
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID, &glance), S_SUCCESS);

  // Request the current slice for this glance; this should match the earliest-expiring slice in
  // the glance we just inserted above
  AppGlanceSliceInternal slice_out;
  cl_assert_equal_b(app_glance_service_get_current_slice(&APP_GLANCE_TEST_UUID, &slice_out), true);
  cl_assert_equal_m(&slice_out, &glance.slices[1], sizeof(slice_out));

  // Let some time "pass" so that the creation time of this next reload doesn't get ignored
  fake_rtc_increment_time(10);

  // Reload the glance using the provided callback; this should empty the slices in the glance
  app_glance_reload(reload_cb, NULL);

  // Read the glance back and check that it doesn't have any slices anymore
  AppGlance glance_read = {};
  cl_assert_equal_i(app_glance_db_read_glance(&APP_GLANCE_TEST_UUID, &glance_read), S_SUCCESS);
  cl_assert_equal_i(glance_read.num_slices, 0);
  for (unsigned int i = 0; i < sizeof(glance_read.slices); i++) {
    const uint8_t byte = ((uint8_t *)glance_read.slices)[i];
    cl_assert_equal_i(byte, 0);
  }

  // Request the current slice for this glance again; this should return false since there aren't
  // any slices in the glance anymore
  cl_assert_equal_b(app_glance_service_get_current_slice(&APP_GLANCE_TEST_UUID, &slice_out), false);
}

void test_app_glance__reload_with_null_callback_empties_slices(void) {
  prv_glance_clear_test(NULL);
}

static void prv_reload_with_no_slices_added_cb(AppGlanceReloadSession *session, size_t limit,
                                               void *context) {
  // We don't add any slices in this callback on purpose
  return;
}

void test_app_glance__reload_with_no_slices_added_empties_slices(void) {
  prv_glance_clear_test(prv_reload_with_no_slices_added_cb);
}
