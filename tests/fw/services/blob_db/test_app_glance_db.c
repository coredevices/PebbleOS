/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/app_glance.h"
#include "drivers/rtc.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/app_glances/app_glance_service.h"
#include "services/normal/blob_db/app_glance_db.h"
#include "services/normal/blob_db/app_glance_db_private.h"
#include "services/normal/filesystem/pfs.h"
#include "util/uuid.h"

// Fakes
////////////////////////////////////////////////////////////////

#include "fake_settings_file.h"
#include "fake_events.h"

// Stubs
////////////////////////////////////////////////////////////////

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

status_t pfs_remove(const char *name) {
  fake_settings_file_reset();
  return S_SUCCESS;
}

static bool s_app_cache_entry_exists = true;
bool app_cache_entry_exists(AppInstallId app_id) {
  return s_app_cache_entry_exists;
}

static int s_launch_count = 0;
status_t app_cache_app_launched(AppInstallId app_id) {
  s_launch_count++;
  return S_SUCCESS;
}

static AppInstallId s_app_install_id = 1;
AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  if (!uuid) {
    return INSTALL_ID_INVALID;
  }
  return s_app_install_id;
}

bool app_install_id_from_system(AppInstallId id) {
  return (id < INSTALL_ID_INVALID);
}

bool app_install_id_from_app_db(AppInstallId id) {
  return (id > INSTALL_ID_INVALID);
}

#define APP_GLANCE_TEST_UUID \
    (UuidMake(0x3d, 0xc6, 0xb9, 0x4c, 0x4, 0x2, 0x48, 0xf4, \
              0xbe, 0x14, 0x81, 0x17, 0xf1, 0xa, 0xa9, 0xc4))

static const uint8_t s_app_glance_basic[] = {
  // Version
  APP_GLANCE_DB_CURRENT_VERSION,
  // Creation time
  0x14, 0x13, 0x4E, 0x57,   // 1464734484 (Tue, 31 May 2016 22:41:24 GMT)

  // Slice 1
  0x22, 0x00,               // Total size
  0x00,                     // AppGlanceSliceType - AppGlanceSliceType_IconAndSubtitle
  0x03,                     // Number of attributes
  // Slice Attributes
  0x25,                     // Attribute ID - AttributeIdTimestamp
  0x04, 0x00,               // Attribute Length
  // Slice expiration time:
  0x94, 0x64, 0x4F, 0x57,   // 1464820884 (Wed, 1 June 2016 22:41:24 GMT)
  0x30,                     // Attribute ID - AttributeIdIcon
  0x04, 0x00,               // Attribute Length
  // Slice icon resource ID:
  0x69, 0x00, 0x00, 0x00,   //
  0x2F,                     // Attribute ID - AttributeIdSubtitleTemplateString
  0x0D, 0x00,               // Attribute Length
  // Slice subtitle:
  'T', 'e', 's', 't', ' ', 's', 'u', 'b', 't', 'i', 't', 'l', 'e',
};

// Note that `APP_GLANCE_DB_MAX_SLICES_PER_GLANCE` is reduced for the unit tests!
static const uint8_t s_app_glance_with_too_many_slices[] = {
  // Version
  APP_GLANCE_DB_CURRENT_VERSION,
  // Creation time
  0x14, 0x13, 0x4E, 0x57,   // 1464734484 (Tue, 31 May 2016 22:41:24 GMT)

  // Slice 1
  0x0B, 0x00,               // Total size
  0x00,                     // AppGlanceSliceType - AppGlanceSliceType_IconAndSubtitle
  0x01,                     // Number of attributes
  // Slice Attributes
  0x25,                     // Attribute ID - AttributeIdTimestamp
  0x04, 0x00,               // Attribute Length
  // Slice expiration time:
  0x94, 0x64, 0x4F, 0x57,   // 1464820884 (Wed, 1 June 2016 22:41:24 GMT)

  // Slice 2
  0x0B, 0x00,               // Total size
  0x00,                     // AppGlanceSliceType - AppGlanceSliceType_IconAndSubtitle
  0x01,                     // Number of attributes
  // Slice Attributes
  0x25,                     // Attribute ID - AttributeIdTimestamp
  0x04, 0x00,               // Attribute Length
  // Slice expiration time:
  0x95, 0x64, 0x4F, 0x57,   // 1464820884 (Wed, 1 June 2016 22:41:25 GMT)

  // Slice 3
  0x0B, 0x00,               // Total size
  0x00,                     // AppGlanceSliceType - AppGlanceSliceType_IconAndSubtitle
  0x01,                     // Number of attributes
  // Slice Attributes
  0x25,                     // Attribute ID - AttributeIdTimestamp
  0x04, 0x00,               // Attribute Length
  // Slice expiration time:
  0x96, 0x64, 0x4F, 0x57,   // 1464820884 (Wed, 1 June 2016 22:41:26 GMT)
};

static const uint8_t s_app_glance_with_invalid_slice_total_sizes[] = {
    // Version
    APP_GLANCE_DB_CURRENT_VERSION,
    // Creation time
    0x14, 0x13, 0x4E, 0x57,   // 1464734484 (Tue, 31 May 2016 22:41:24 GMT)

    // Slice 1 (valid)
    0x0B, 0x00,               // Total size
    0x00,                     // AppGlanceSliceType - AppGlanceSliceType_IconAndSubtitle
    0x01,                     // Number of attributes
    // Slice Attributes
    0x25,                     // Attribute ID - AttributeIdTimestamp
    0x04, 0x00,               // Attribute Length
    // Slice expiration time:
    0x94, 0x64, 0x4F, 0x57,   // 1464820884 (Wed, 1 June 2016 22:41:24 GMT)

    // Slice 2 (invalid total_size)
    0xFF, 0x00,               // Total size
    0x00,                     // AppGlanceSliceType - AppGlanceSliceType_IconAndSubtitle
    0x01,                     // Number of attributes
    // Slice Attributes
    0x25,                     // Attribute ID - AttributeIdTimestamp
    0x04, 0x00,               // Attribute Length
    // Slice expiration time:
    0x95, 0x64, 0x4F, 0x57,   // 1464820884 (Wed, 1 June 2016 22:41:25 GMT)
};

// Setup
////////////////////////////////////////////////////////////////

void test_app_glance_db__initialize(void) {
  s_app_cache_entry_exists = true;
  s_app_install_id = 1;
  s_launch_count = 0;

  fake_event_init();
  fake_settings_file_reset();
  app_glance_db_init();
}

void app_glance_db_deinit(void);

void test_app_glance_db__cleanup(void) {
  app_glance_db_deinit();
}

// Blob Tests
////////////////////////////////////////////////////////////////

void test_app_glance_db__blob_insertion_with_invalid_key_or_val_length_fails(void) {
  // Invalid key length should fail
  const size_t invalid_key_length = 1337;
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, invalid_key_length,
                                         (uint8_t *)&s_app_glance_basic,
                                         sizeof(s_app_glance_basic)),
                    E_INVALID_ARGUMENT);

  // Invalid val length should fail
  const size_t invalid_val_size = sizeof(SerializedAppGlanceHeader) - 1;
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&s_app_glance_basic, invalid_val_size),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__basic_glance_blob_insert_and_read(void) {
  const size_t glance_size = sizeof(s_app_glance_basic);

  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&s_app_glance_basic, glance_size),
                    S_SUCCESS);
  cl_assert_equal_i(app_glance_db_get_len((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE),
                    glance_size);

  uint8_t *glance_out = kernel_malloc(glance_size);
  cl_assert_equal_i(app_glance_db_read((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE, glance_out,
                                       glance_size),
                    S_SUCCESS);
  cl_assert_equal_m(glance_out, (uint8_t *)s_app_glance_basic, glance_size);
  kernel_free(glance_out);
}

void test_app_glance_db__blob_read_with_invalid_key_length_or_null_val_out_fails(void) {
  // Call the basic glance blob insert test to insert the basic glance blob
  test_app_glance_db__basic_glance_blob_insert_and_read();

  const size_t glance_size = sizeof(s_app_glance_basic);
  uint8_t glance_out[glance_size];

  // Trying to read the basic glance blob back with an invalid key length should fail
  const size_t invalid_key_length = 1337;
  cl_assert_equal_i(app_glance_db_read((uint8_t *)&APP_GLANCE_TEST_UUID, invalid_key_length,
                                       glance_out, glance_size),
                    E_INVALID_ARGUMENT);

  // Trying to read the basic glance blob back with a NULL glance_out argument should fail
  uint8_t *invalid_glance_out = NULL;
  cl_assert_equal_i(app_glance_db_read((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                       invalid_glance_out, glance_size),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__basic_glance_blob_delete(void) {
  // Call the basic glance blob insert test to insert the basic glance blob
  test_app_glance_db__basic_glance_blob_insert_and_read();

  // Delete the basic glance blob
  cl_assert_equal_i(app_glance_db_delete((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE),
                    S_SUCCESS);

  const size_t glance_size = sizeof(s_app_glance_basic);
  uint8_t glance_out[glance_size];

  // Trying to read the basic glance blob now should fail because it should no longer exist
  cl_assert_equal_i(app_glance_db_read((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE, glance_out,
                                       glance_size),
                    E_DOES_NOT_EXIST);
}

void test_app_glance_db__delete_non_existing_blob_does_nothing(void) {
  // Trying to delete a glance that is not actually in the database should do nothing
  cl_assert_equal_i(app_glance_db_delete((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE),
                    S_SUCCESS);
}

void test_app_glance_db__delete_blob_with_invalid_key_length_fails(void) {
  // Call the basic glance blob insert test to insert the basic glance blob
  test_app_glance_db__basic_glance_blob_insert_and_read();

  // Trying to delete the basic glance blob with an invalid key length should fail
  const size_t invalid_key_length = 1337;
  cl_assert_equal_i(app_glance_db_delete((uint8_t *)&APP_GLANCE_TEST_UUID, invalid_key_length),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__glance_blob_with_older_creation_time_than_existing_not_inserted(void) {
  // Insert the first glance blob
  SerializedAppGlanceHeader app_glance_1 = (SerializedAppGlanceHeader) {
    .version = APP_GLANCE_DB_CURRENT_VERSION,
    .creation_time = 1464734484, // Tue, 31 May 2016 22:41:24 GMT
  };
  const size_t glance_1_size = sizeof(app_glance_1);
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&app_glance_1, glance_1_size),
                    S_SUCCESS);
  cl_assert_equal_i(app_glance_db_get_len((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE),
                    glance_1_size);

  // Try to insert a different glance blob with an older creation time; this should fail
  SerializedAppGlanceHeader app_glance_2 = (SerializedAppGlanceHeader) {
    .version = APP_GLANCE_DB_CURRENT_VERSION,
    .creation_time = 1464648084, // Mon, 30 May 2016 22:41:24 GMT
  };
  const size_t glance_2_size = sizeof(app_glance_2);
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&app_glance_2, glance_2_size),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__glance_blob_with_too_many_slices_inserted_but_trimmed(void) {
  const size_t original_glance_size = sizeof(s_app_glance_with_too_many_slices);
  const size_t excess_slices_size = 11;
  const size_t trimmed_glance_size = original_glance_size - excess_slices_size;

  // Insert the glance blob with too many slices; this should succeed
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&s_app_glance_with_too_many_slices,
                                         original_glance_size),
                    S_SUCCESS);
  // But the length we read back should be trimmed of the excess slices
  cl_assert_equal_i(app_glance_db_get_len((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE),
                    trimmed_glance_size);

  // The glance blob read back from the database should match everything up to where we trimmed
  uint8_t glance_out[trimmed_glance_size];
  cl_assert_equal_i(app_glance_db_read((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE, glance_out,
                                       trimmed_glance_size),
                    S_SUCCESS);
  cl_assert_equal_m(glance_out, (uint8_t *)s_app_glance_with_too_many_slices, trimmed_glance_size);
}

static void prv_check_invalid_version_code_blob_not_inserted(uint8_t version) {
  const SerializedAppGlanceHeader app_glance = (SerializedAppGlanceHeader) {
    .version = version,
  };

  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&app_glance, sizeof(app_glance)),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__lower_version_blob_not_inserted(void) {
  for (uint8_t version = 0; version < APP_GLANCE_DB_CURRENT_VERSION; version++) {
    prv_check_invalid_version_code_blob_not_inserted(version);
  }
}

void test_app_glance_db__higher_version_not_blob_inserted(void) {
  prv_check_invalid_version_code_blob_not_inserted(APP_GLANCE_DB_CURRENT_VERSION + 1);
}

static status_t prv_insert_dummy_glance_blob_with_size(uint16_t blob_size) {
  const uint8_t dummy_app_glance[] = {
    // Version
    APP_GLANCE_DB_CURRENT_VERSION,
    // Creation time
    0x14, 0x13, 0x4E, 0x57,   // 1464734484 (Tue, 31 May 2016 22:41:24 GMT)

    // Slice 1
    (uint8_t)(blob_size & 0xFF), (uint8_t)(blob_size >> 8), // Total size
  };
  return app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                              (uint8_t *)&dummy_app_glance, sizeof(dummy_app_glance));
}

void test_app_glance_db__check_too_small_blob_not_inserted(void) {
  cl_assert_equal_i(prv_insert_dummy_glance_blob_with_size(APP_GLANCE_DB_SLICE_MIN_SIZE - 1),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__check_too_large_blob_not_inserted(void) {
  cl_assert_equal_i(prv_insert_dummy_glance_blob_with_size(APP_GLANCE_DB_SLICE_MAX_SIZE + 1),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__check_invalid_slice_total_sizes_blob_not_inserted(void) {
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&s_app_glance_with_invalid_slice_total_sizes,
                                         sizeof(s_app_glance_with_invalid_slice_total_sizes)),
                    E_INVALID_ARGUMENT);
}

status_t app_glance_db_insert_stale(const uint8_t *key, int key_len, const uint8_t *val,
                                    int val_len);

void test_app_glance_db__read_stale_glance_blob(void) {
  // Force the insertion of a stale glance blob (outdated version)
  const SerializedAppGlanceHeader app_glance = (SerializedAppGlanceHeader) {
    .version = APP_GLANCE_DB_CURRENT_VERSION - 1,
  };
  cl_assert_equal_i(app_glance_db_insert_stale((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                               (uint8_t *)&app_glance, sizeof(app_glance)),
                    S_SUCCESS);

  // Verify that trying to read the blob back fails due to it not existing
  SerializedAppGlanceHeader glance_out = {};
  cl_assert_equal_i(app_glance_db_read((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                       (uint8_t *)&glance_out, sizeof(SerializedAppGlanceHeader)),
                    E_DOES_NOT_EXIST);
}

void test_app_glance_db__glance_blob_with_slice_missing_expiration_time_gets_default_value(void) {
  const uint8_t app_glance_with_slice_missing_expiration_time[] = {
      // Version
      APP_GLANCE_DB_CURRENT_VERSION,
      // Creation time
      0x14, 0x13, 0x4E, 0x57,   // 1464734484 (Tue, 31 May 2016 22:41:24 GMT)

      // Slice 1
      0x1B, 0x00,               // Total size
      0x00,                     // AppGlanceSliceType - AppGlanceSliceType_IconAndSubtitle
      0x02,                     // Number of attributes
      // Slice Attributes
      0x30,                     // Attribute ID - AttributeIdIcon
      0x04, 0x00,               // Attribute Length
      // Slice icon resource ID:
      0x69, 0x00, 0x00, 0x00,   //
      0x2F,                     // Attribute ID - AttributeIdSubtitleTemplateString
      0x0D, 0x00,               // Attribute Length
      // Slice subtitle:
      'T', 'e', 's', 't', ' ', 's', 'u', 'b', 't', 'i', 't', 'l', 'e',
  };
  const size_t app_glance_with_slice_missing_expiration_time_size =
      sizeof(app_glance_with_slice_missing_expiration_time);

  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
                                         (uint8_t *)&app_glance_with_slice_missing_expiration_time,
                                         app_glance_with_slice_missing_expiration_time_size),
                    S_SUCCESS);

  AppGlance read_back_glance = {};
  cl_assert_equal_i(app_glance_db_read_glance(&APP_GLANCE_TEST_UUID, &read_back_glance), S_SUCCESS);
  cl_assert_equal_i(read_back_glance.slices[0].expiration_time, APP_GLANCE_SLICE_NO_EXPIRATION);
}

// Glance Tests
////////////////////////////////////////////////////////////////

void test_app_glance_db__basic_glance_insert_and_read(void) {
  const AppGlance glance = (AppGlance) {
    .num_slices = 2,
    .slices = {
      {
        .expiration_time = 1464734484, // (Tue, 31 May 2016 22:41:24 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .icon_resource_id = RESOURCE_ID_SETTINGS_ICON_AIRPLANE,
          .template_string = "Test subtitle",
        },
      },
      {
        .expiration_time = 1465579430, // (Fri, 10 Jun 2016 17:23:50 GMT)
        .type = AppGlanceSliceType_IconAndSubtitle,
        .icon_and_subtitle = {
          .icon_resource_id = RESOURCE_ID_SETTINGS_ICON_BLUETOOTH,
          .template_string = "Test subtitle 2",
        },
      },
    },
  };
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID, &glance), S_SUCCESS);

  AppGlance read_back_glance = {};
  cl_assert_equal_i(app_glance_db_read_glance(&APP_GLANCE_TEST_UUID, &read_back_glance),
                    S_SUCCESS);
  cl_assert_equal_m(&glance, &read_back_glance, sizeof(AppGlance));
}

void test_app_glance_db__reading_nonexistent_glance_returns_does_not_exist(void) {
  AppGlance glance = {};
  cl_assert_equal_i(app_glance_db_read_glance(&UUID_INVALID, &glance), E_DOES_NOT_EXIST);
}

void test_app_glance_db__reading_glance_with_invalid_arguments_fails(void) {
  // NULL UUID fails
  AppGlance glance_out = {};
  cl_assert_equal_i(app_glance_db_read_glance(NULL, &glance_out), E_INVALID_ARGUMENT);

  // NULL glance_out fails
  cl_assert_equal_i(app_glance_db_read_glance(&APP_GLANCE_TEST_UUID, NULL), E_INVALID_ARGUMENT);
}

void test_app_glance_db__inserting_glance_with_invalid_arguments_fails(void) {
  // NULL UUID fails
  const AppGlance glance = {};
  cl_assert_equal_i(app_glance_db_insert_glance(NULL, &glance), E_INVALID_ARGUMENT);

  // NULL glance fails
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID, NULL), E_INVALID_ARGUMENT);

  // Glance with too many slices fails
  const AppGlance glance_with_too_many_slices = (AppGlance) {
    .num_slices = 1337,
  };
  cl_assert(glance_with_too_many_slices.num_slices > APP_GLANCE_DB_MAX_SLICES_PER_GLANCE);
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID,
                                                &glance_with_too_many_slices), E_INVALID_ARGUMENT);

  // Glance containing a slice with an invalid type fails
  const AppGlance glance_containing_slice_with_invalid_type = (AppGlance) {
    .num_slices = 1,
    .slices = {
      {
        .expiration_time = 1464734484, // (Tue, 31 May 2016 22:41:24 GMT)
        .type = (AppGlanceSliceType)200,
      },
    },
  };
  cl_assert(glance_containing_slice_with_invalid_type.slices[0].type >= AppGlanceSliceTypeCount);
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID,
                                                &glance_containing_slice_with_invalid_type),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__read_glance_creation_time(void) {
  time_t time_out;

  // Reading the creation time of a glance that doesn't exist should return "does not exist"
  cl_assert_equal_i(app_glance_db_read_creation_time(&APP_GLANCE_TEST_UUID, &time_out),
                    E_DOES_NOT_EXIST);

  // Insert a glance and check that the creation time we read back matches
  test_app_glance_db__basic_glance_blob_insert_and_read();
  cl_assert_equal_i(app_glance_db_read_creation_time(&APP_GLANCE_TEST_UUID, &time_out),
                    S_SUCCESS);
  cl_assert_equal_i(time_out, 1464734484);
}

void test_app_glance_db__read_glance_creation_time_with_invalid_arguments_fails(void) {
  // NULL UUID fails
  time_t time_out;
  cl_assert_equal_i(app_glance_db_read_creation_time(NULL, &time_out), E_INVALID_ARGUMENT);

  // NULL time_out fails
  cl_assert_equal_i(app_glance_db_read_creation_time(&APP_GLANCE_TEST_UUID, NULL),
                    E_INVALID_ARGUMENT);
}

void test_app_glance_db__empty_glance_insert_after_basic_glance_insert_succeeds(void) {
  // Call the basic glance insert test to insert the basic glance
  test_app_glance_db__basic_glance_insert_and_read();

  // Let some time pass so the creation time of this next glance insertion is newer
  rtc_set_time(rtc_get_time() + 10);

  // Try inserting an empty glance; this should succeed and clear the glance
  AppGlance empty_glance = {};
  cl_assert_equal_i(app_glance_db_insert_glance(&APP_GLANCE_TEST_UUID, &empty_glance), S_SUCCESS);
  AppGlance read_back_glance = {};
  cl_assert_equal_i(app_glance_db_read_glance(&APP_GLANCE_TEST_UUID, &read_back_glance),
                    S_SUCCESS);
  cl_assert_equal_m(&empty_glance, &read_back_glance, sizeof(AppGlance));
}

void test_app_glance_db__insert_no_app_installed(void) {
  s_app_install_id = INSTALL_ID_INVALID;
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
      s_app_glance_basic, sizeof(s_app_glance_basic)), E_DOES_NOT_EXIST);
}

void test_app_glance_db__insert_app_not_in_cache(void)  {
  s_app_install_id = 10;
  s_app_cache_entry_exists = false;
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
      s_app_glance_basic, sizeof(s_app_glance_basic)), S_SUCCESS);

  cl_assert_equal_i(fake_event_get_count(), 1);
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_APP_FETCH_REQUEST_EVENT);
  cl_assert(!e.app_fetch_request.with_ui);
  cl_assert_equal_i(e.app_fetch_request.id, 10);
}

void test_app_glance_db__insert_app_in_cache(void) {
  cl_assert_equal_i(app_glance_db_insert((uint8_t *)&APP_GLANCE_TEST_UUID, UUID_SIZE,
      s_app_glance_basic, sizeof(s_app_glance_basic)), S_SUCCESS);
  cl_assert_equal_i(s_launch_count, 1);
}
