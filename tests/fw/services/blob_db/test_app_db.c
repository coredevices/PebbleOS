/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "process_management/app_install_types.h"
#include "process_management/pebble_process_info.h"
#include "process_management/pebble_process_md.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/blob_db/app_db.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_app_cache.h"
#include "stubs_events.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

void app_install_clear_app_db(void) {
}

void put_bytes_cancel(void) {
}

typedef void (*InstallCallbackDoneCallback)(void*);
bool app_install_do_callbacks(InstallEventType event_type, AppInstallId install_id, Uuid *uuid,
    InstallCallbackDoneCallback done_callback, void* done_callback_data) {
  return true;
}

bool app_fetch_in_progress(void) {
  return false;
}

void app_fetch_cancel_from_system_task(void) {
}

extern AppInstallId app_db_check_next_unique_id(void);

static const AppDBEntry app1 = {
  .name = "Application 1",
  .uuid = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
         0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4},
  .app_version = {
    .major = 1,
    .minor = 1,
  },
  .sdk_version = {
    .major = 1,
    .minor = 1,
  },
  .info_flags = 0,
  .icon_resource_id = 0,
};

static const AppDBEntry app2 = {
  .name = "Application 2",
  .uuid = {0x55, 0xcb, 0x7c, 0x75, 0x8a, 0x35, 0x44, 0x87,
           0x90, 0xa4, 0x91, 0x3f, 0x1f, 0xa6, 0x76, 0x01},
  .app_version = {
    .major = 1,
    .minor = 1,
  },
  .sdk_version = {
    .major = 1,
    .minor = 1,
  },
  .info_flags = 0,
  .icon_resource_id = 0,
};

static const AppDBEntry app3 = {
  .name = "Application 3",
  .uuid = {0x7c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c,
           0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2, 0x05},
  .app_version = {
    .major = 1,
    .minor = 1,
  },
  .sdk_version = {
    .major = 1,
    .minor = 1,
  },
  .info_flags = 0,
  .icon_resource_id = 0,
};


// Setup
////////////////////////////////////////////////////////////////

void test_app_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  app_db_init();

  // add all three
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app1.uuid,
      sizeof(Uuid), (uint8_t*)&app1, sizeof(AppDBEntry)));

  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app2.uuid,
      sizeof(Uuid), (uint8_t*)&app2, sizeof(AppDBEntry)));

  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app3.uuid,
      sizeof(Uuid), (uint8_t*)&app3, sizeof(AppDBEntry)));
}

void test_app_db__cleanup(void) {
  //nada
}

// Tests
////////////////////////////////////////////////////////////////

void test_app_db__basic_test(void) {
  // confirm all three are there
  cl_assert(app_db_get_len((uint8_t*)&app1.uuid, sizeof(Uuid)) > 0);
  cl_assert(app_db_get_len((uint8_t*)&app2.uuid, sizeof(Uuid)) > 0);
  cl_assert(app_db_get_len((uint8_t*)&app3.uuid, sizeof(Uuid)) > 0);

  // remove #1 and confirm it's deleted
  cl_assert_equal_i(S_SUCCESS, app_db_delete((uint8_t*)&app1.uuid, sizeof(Uuid)));
  cl_assert_equal_i(0, app_db_get_len((uint8_t*)&app1.uuid, sizeof(Uuid)));

  // add 1 back so it's clean
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app1.uuid,
      sizeof(Uuid), (uint8_t*)&app1, sizeof(AppDBEntry)));

  AppDBEntry temp;
  cl_assert_equal_i(S_SUCCESS, app_db_read((uint8_t*)&app1.uuid,
      sizeof(Uuid), (uint8_t*)&temp, sizeof(AppDBEntry)));

  cl_assert_equal_i(5, app_db_check_next_unique_id());

  // check app 1
  memset(&temp, 0, sizeof(AppDBEntry));
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_uuid(&app1.uuid, &temp));
  cl_assert_equal_b(true, uuid_equal(&app1.uuid, &temp.uuid));
  cl_assert_equal_i(0 , strncmp((char *)&app1.name, (char *)&temp.name, APP_NAME_SIZE_BYTES));

  // check app 2
  memset(&temp, 0, sizeof(AppDBEntry));
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_uuid(&app1.uuid, &temp));
  cl_assert_equal_b(true, uuid_equal(&app1.uuid, &temp.uuid));
  cl_assert_equal_i(0, strncmp((char *)&app1.name, (char *)&temp.name, APP_NAME_SIZE_BYTES));

  // check app 3
  memset(&temp, 0, sizeof(AppDBEntry));
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_uuid(&app3.uuid, &temp));
  cl_assert_equal_b(true, uuid_equal(&app3.uuid, &temp.uuid));
  cl_assert_equal_i(0, strncmp((char *)&app3.name, (char *)&temp.name, APP_NAME_SIZE_BYTES));
}

void test_app_db__retrieve_app_db_entries_by_install_id(void) {
  AppDBEntry temp;

  // check app_db_get_install_id_for_uuid for app 1
  memset(&temp, 0, sizeof(AppDBEntry));
  AppInstallId app_one_id = app_db_get_install_id_for_uuid(&app1.uuid);
  cl_assert(app_one_id > 0);
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_install_id(app_one_id, &temp));
  cl_assert_equal_b(true, uuid_equal(&app1.uuid, &temp.uuid));

  // check app_db_get_install_id_for_uuid for app 2
  memset(&temp, 0, sizeof(AppDBEntry));
  AppInstallId app_two_id = app_db_get_install_id_for_uuid(&app2.uuid);
  cl_assert(app_one_id > 0);
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_install_id(app_two_id, &temp));
  cl_assert_equal_b(true, uuid_equal(&app2.uuid, &temp.uuid));

  // check app_db_get_install_id_for_uuid for app 3
  memset(&temp, 0, sizeof(AppDBEntry));
  AppInstallId app_three_id = app_db_get_install_id_for_uuid(&app3.uuid);
  cl_assert(app_one_id > 0);
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_install_id(app_three_id, &temp));
  cl_assert_equal_b(true, uuid_equal(&app3.uuid, &temp.uuid));
}

void test_app_db__retrieve_app_db_entries_by_uuid(void) {
  AppDBEntry temp;

  // check app_db_get_install_id_for_uuid for app 1
  memset(&temp, 0, sizeof(AppDBEntry));
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_uuid(&app1.uuid, &temp));
  cl_assert_equal_b(true, uuid_equal(&app1.uuid, &temp.uuid));

  // check app_db_get_install_id_for_uuid for app 2
  memset(&temp, 0, sizeof(AppDBEntry));
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_uuid(&app2.uuid, &temp));
  cl_assert_equal_b(true, uuid_equal(&app2.uuid, &temp.uuid));

  // check app_db_get_install_id_for_uuid for app 3
  memset(&temp, 0, sizeof(AppDBEntry));
  cl_assert_equal_i(S_SUCCESS, app_db_get_app_entry_for_uuid(&app3.uuid, &temp));
  cl_assert_equal_b(true, uuid_equal(&app3.uuid, &temp.uuid));
}

void test_app_db__overwrite(void) {
  // add 3 of the same. Confirm that the entry was overwritten by checking next ID.
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app1.uuid, sizeof(Uuid), (uint8_t*)&app1,
      sizeof(AppDBEntry)));
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app1.uuid, sizeof(Uuid), (uint8_t*)&app1,
      sizeof(AppDBEntry)));
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app1.uuid, sizeof(Uuid), (uint8_t*)&app1,
      sizeof(AppDBEntry)));
  cl_assert(app_db_check_next_unique_id() == 4);

  // add two more duplicates of a different app. Confirm it only increments by 1.
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app2.uuid, sizeof(Uuid), (uint8_t*)&app2,
      sizeof(AppDBEntry)));
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t*)&app2.uuid, sizeof(Uuid), (uint8_t*)&app2,
      sizeof(AppDBEntry)));
  cl_assert(app_db_check_next_unique_id() == 4);
}

void test_app_db__test_exists(void) {
  cl_assert_equal_b(false, app_db_exists_install_id(-1));
  cl_assert_equal_b(false, app_db_exists_install_id(0));
  cl_assert_equal_b(true, app_db_exists_install_id(1));
  cl_assert_equal_b(true, app_db_exists_install_id(2));
  cl_assert_equal_b(true, app_db_exists_install_id(3));
  cl_assert_equal_b(false, app_db_exists_install_id(4));
}

static const uint8_t some_data[] = {0x01, 0x02, 0x17, 0x54};

void prv_enumerate_entries(AppInstallId install_id, AppDBEntry *entry, void *data) {
  switch(install_id) {
    case 1:
      cl_assert_equal_m(&app1, entry, sizeof(AppDBEntry));
      break;
    case 2:
      cl_assert_equal_m(&app2, entry, sizeof(AppDBEntry));
      break;
    case 3:
      cl_assert_equal_m(&app3, entry, sizeof(AppDBEntry));
      break;
    default:
      break;
  }
  cl_assert_equal_m((uint8_t *)some_data, (uint8_t *)data, sizeof(some_data));
}

void test_app_db__enumerate(void) {
  app_db_enumerate_entries(prv_enumerate_entries, (void *)&some_data);
}
