/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/persist_map.h"
#include "applib/persist_private.h"
#include "comm/ble/app_profiles/ancs_app_storage.h"
#include "drivers/crc.h"
#include "kernel/services/file.h"
#include "system/filesystem.h"
#include "system/logging.h"
#include "util/size.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Stubs
////////////////////////////////////

#include "stubs_passert.h"
#include "stubs_serial.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_system_reset.h"
#include "stubs_task_watchdog.h"

// Fakes
////////////////////////////////////

#include "fake_session.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"

// Tests
////////////////////////////////////

#define TEST_START FILESYSTEM_FILE_TEST_SPACE_BEGIN
#define TEST_SIZE (FILESYSTEM_FILE_TEST_SPACE_END - \
  FILESYSTEM_FILE_TEST_SPACE_BEGIN)

void test_ancs_app_storage__initialize(void) {
  fake_spi_flash_init(TEST_START, TEST_SIZE);
  file_system_format();
  file_system_reset();
  ancs_app_storage_init();
}

void test_ancs_app_storage__cleanup(void) {
  ancs_app_storage_deinit();
}

void test_ancs_app_storage__one_app(void) {
  ANCSAppData app_data = {
    .bundle_id = "com.getpebble.PebbleApp",
    .display_name = "Pebble",
    .flags = ANCSAppIsBlocked,
  };
  app_data.is_meta_changed = true;
  ancs_app_storage_save(&app_data);

  ANCSAppData app_data_out = { 0 };
  ancs_app_storage_load(app_data.bundle_id, &app_data_out);

  cl_assert_equal_s(app_data.bundle_id, app_data_out.bundle_id);
  cl_assert_equal_i(app_data.flags, app_data_out.flags);
  cl_assert_equal_s(app_data.display_name, app_data_out.display_name);

  ancs_app_destroy_buffer(&app_data_out);
}

void test_ancs_app_storage__overwrite(void) {
  ANCSAppData app_data = {
    .bundle_id = "com.getpebble.PebbleApp",
    .display_name = "Pebble",
    .flags = ANCSAppIsBlocked,
  };
  app_data.is_meta_changed = true;
  ancs_app_storage_save(&app_data);

  app_data.display_name = "Pebble 2";
  app_data.is_meta_changed = true;
  ancs_app_storage_save(&app_data);

  ANCSAppData app_data_out = { 0 };
  ancs_app_storage_load(app_data.bundle_id, &app_data_out);

  cl_assert_equal_s(app_data.bundle_id, app_data_out.bundle_id);
  cl_assert_equal_i(app_data.flags, app_data_out.flags);
  cl_assert_equal_s(app_data.display_name, app_data_out.display_name);

  ancs_app_destroy_buffer(&app_data_out);
}

static uint32_t get_key(const char* bundle_id) {
  return legacy_defective_checksum_memory(bundle_id, strlen(bundle_id));
}

void test_ancs_app_storage__hash_collisions(void) {
  ANCSAppData app_data = {
    .flags = ANCSAppFlagNone,
  };

  // Courtesy of http://programmers.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed
  char* collide_pairs[][2] = {
    { "codding", "gnu" },
    { "exhibiters", "schlager" },
  };

  for (unsigned int i = 0; i < ARRAY_LENGTH(collide_pairs); ++i) {
    for (unsigned int j = 0; j < 2; ++j) {
      char* name = collide_pairs[i][j];
      app_data.bundle_id = name;
      app_data.display_name = name;
      app_data.is_meta_changed = true;
      uint32_t key = get_key(name);
      PBL_LOG(LOG_LEVEL_DEBUG, "name: %s, key: %u", name, (unsigned) key);
      ancs_app_storage_save(&app_data);
    }
  }

  ANCSAppData app_data_out = { 0 };
  for (unsigned int i = 0; i < ARRAY_LENGTH(collide_pairs); ++i) {
    for (unsigned int j = 0; j < 2; ++j) {
      char* name = collide_pairs[i][j];
      app_data.bundle_id = name;
      app_data.display_name = name;
      ancs_app_storage_load(name, &app_data_out);

      cl_assert_equal_s(app_data.bundle_id, app_data_out.bundle_id);
      cl_assert_equal_i(app_data.flags, app_data_out.flags);
      cl_assert_equal_s(app_data.display_name, app_data_out.display_name);

      ancs_app_destroy_buffer(&app_data_out);
    }
  }
}

void test_ancs_app_storage__iter(void) {
  ANCSAppData apps[] = {
    { .bundle_id = "com.apple.MobileSMS", .display_name = "Messages" },
    { .bundle_id = "com.apple.facetime", .display_name = "FaceTime" },
    { .bundle_id = "com.facebook.Messenger", .display_name = "Facebook" },
    { .bundle_id = "com.atebits.Tweetie2", .display_name = "Twitter" },
    { .bundle_id = "com.apple.mobilecal", .display_name = "Calender" },
    { .bundle_id = "com.blackberry.bbm1", .display_name = "BBM" },
    { .bundle_id = "net.whatsapp.WhatsApp", .display_name = "WhatsApp" },
    { .bundle_id = "com.toyopagroup.picaboo", .display_name = "Snapchat" },
    { .bundle_id = "com.kik.chat", .display_name = "Kik Chat" },
    { .bundle_id = "com.apple.mobilemail", .display_name = "Mail" },
    { .bundle_id = "com.yahoo.Aerogram", .display_name = "YMail" },
    { .bundle_id = "co.inboxapp.inbox", .display_name = "Inbox" },
    { .bundle_id = "com.google.Gmail", .display_name = "Gmail" },
  };

  for (unsigned int i = 0; i < ARRAY_LENGTH(apps); ++i) {
    apps[i].is_meta_changed = true;
    apps[i].is_local_changed = true;
    ancs_app_storage_save(&apps[i]);
  }

  ANCSAppData app_data_out = { 0 };
  for (unsigned int i = 0; i < ARRAY_LENGTH(apps); ++i) {
    PBL_LOG(LOG_LEVEL_DEBUG, "i: %d, name: %s", i, apps[i].bundle_id);
    ancs_app_storage_load(apps[i].bundle_id, &app_data_out);

    cl_assert_equal_s(apps[i].bundle_id, app_data_out.bundle_id);
    cl_assert_equal_i(apps[i].flags, app_data_out.flags);
    cl_assert_equal_s(apps[i].display_name, app_data_out.display_name);

    ancs_app_destroy_buffer(&app_data_out);
  }

  ancs_app_storage_iter_begin();
  for (unsigned int i = 0; ancs_app_storage_next(&app_data_out); ++i) {
    PBL_LOG(LOG_LEVEL_DEBUG, "i: %d, name: %s, name_out: %s", i, apps[i].bundle_id, app_data_out.bundle_id);
    cl_assert_equal_s(apps[i].bundle_id, app_data_out.bundle_id);
  }
}

