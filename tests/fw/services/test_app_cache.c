/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/app_cache.h"

#include "process_management/app_install_manager.h"
#include "resource/resource_storage.h"
#include "process_management/app_install_types.h"
#include "services/normal/filesystem/app_file.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/process_management/app_storage.h"
#include "services/normal/settings/settings_file.h"
#include "shell/normal/quick_launch.h"
#include <util/size.h>
#include "system/logging.h"
#include "util/attributes.h"
#include <stdio.h>

// Fakes
////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_events.h"

// Stubs
////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_passert.h"
#include "stubs_task_watchdog.h"

void app_storage_delete_app(AppInstallId id) {
  char buffer[30];
  itoa_int(id, buffer, 10);
  pfs_remove(buffer);
}

bool app_storage_app_exists(AppInstallId id) {
  return true;
}

AppInstallId s_test_id_ql_up;
AppInstallId s_test_id_ql_down;
AppInstallId s_test_id_watchface;
AppInstallId s_test_id_worker;

AppInstallId quick_launch_get_app(ButtonId button) {
  if (button == BUTTON_ID_UP) {
    return s_test_id_ql_up;
  } else {
    return s_test_id_ql_down;
  }
}

AppInstallId watchface_get_default_install_id(void) {
  return s_test_id_watchface;
}

AppInstallId worker_preferences_get_default_worker(void) {
  return s_test_id_worker;
}

extern AppInstallId app_cache_get_next_eviction(void);

/* Start of test */

typedef struct {
  AppInstallId id;
  uint32_t size;
  uint32_t priority;
} AppData;

static const AppData app1 = {
  .id = 1,
  .size = 1000,
};

static const AppData app2 = {
  .id = 2,
  .size = 1000,
};

static const AppData app3 = {
  .id = 3,
  .size = 1000,
};

void test_app_cache__initialize(void) {
  rtc_set_time(1478397600);
  fake_spi_flash_init(0, 0x1000000);
  fake_event_init();
  pfs_init(false);
  app_cache_init();
  app_cache_flush();
}

void test_app_cache__cleanup(void) {
  fake_system_task_callbacks_cleanup();

  s_test_id_ql_up = 0;
  s_test_id_ql_down = 0;
  s_test_id_watchface = 0;
  s_test_id_worker = 0;
}

/*************************************
 * Add one and evict it *
 *************************************/

void test_app_cache__easy_evict(void) {
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app1.id, app1.size));
  cl_assert_equal_i(app1.id, app_cache_get_next_eviction());
}

/*************************************
 * Add 3, remove 2, evict one *
 *************************************/

void test_app_cache__add_remove_evict(void) {
  // add all three
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app1.id, app1.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app2.id, app2.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app3.id, app3.size));

  // remove 2
  cl_assert_equal_i(S_SUCCESS, app_cache_remove_entry(app1.id));
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_APP_CACHE_EVENT);
  cl_assert_equal_i(e.app_cache_event.cache_event_type, PebbleAppCacheEvent_Removed);
  cl_assert_equal_i(e.app_cache_event.install_id, app1.id);
  cl_assert_equal_i(S_SUCCESS, app_cache_remove_entry(app3.id));
  e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_APP_CACHE_EVENT);
  cl_assert_equal_i(e.app_cache_event.cache_event_type, PebbleAppCacheEvent_Removed);
  cl_assert_equal_i(e.app_cache_event.install_id, app3.id);

  cl_assert_equal_i(fake_event_get_count(), 2);

  // ensure the only one remaining is the one evicted
  cl_assert_equal_i(app2.id, app_cache_get_next_eviction());
  cl_assert_equal_i(fake_event_get_count(), 2);
}

/*************************************
 * Add 3, update 2, evict 1 *
 *************************************/

void test_app_cache__add_update_evict(void) {
  // add all three
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app1.id, app1.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app2.id, app2.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app3.id, app3.size));

  time_t now = rtc_get_time();
  rtc_set_time(now + 2);

  // remove 2
  cl_assert_equal_i(S_SUCCESS, app_cache_app_launched(app1.id));
  cl_assert_equal_i(S_SUCCESS, app_cache_app_launched(app3.id));

  // ensure the one that has the lowest priority is evicted
  cl_assert_equal_i(app2.id, app_cache_get_next_eviction());
}

/*************************************
 * Add 3, remove 3, evict INVALID_ID *
 *************************************/

void test_app_cache__add_remove_all_evict(void) {
  // add all three
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app1.id, app1.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app2.id, app2.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app3.id, app3.size));

  // remove 3
  cl_assert_equal_i(S_SUCCESS, app_cache_remove_entry(app1.id));
  cl_assert_equal_i(S_SUCCESS, app_cache_remove_entry(app2.id));
  cl_assert_equal_i(S_SUCCESS, app_cache_remove_entry(app3.id));

  cl_assert_equal_i(fake_event_get_count(), 3);

  // ensure the one that is evicted is INVALID_ID
  cl_assert_equal_i(INSTALL_ID_INVALID, app_cache_get_next_eviction());
}

/*************************************
 * Add lots, update lots, update one a little less *
 *************************************/

void test_app_cache__update_all_lots_evict_one(void) {
  static const uint8_t DESIRED_EVICT_ID = 5;
  static const uint8_t NUM_ITEMS = 10;
  static const uint16_t NUM_UPDATES = 10;

  for (int i = 1; i <= NUM_ITEMS; i++) {
    cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(i, 0));
  }

  // repeat LOOPS times
  for (int i = 1; i <= NUM_UPDATES; i++) {
    // go through list and launch each app once.
    for (int j = 1; j <= NUM_ITEMS; j++) {

      if ((i == NUM_UPDATES) && (j == DESIRED_EVICT_ID)) {
        continue;
      }

      cl_assert_equal_i(S_SUCCESS, app_cache_app_launched(j));
    }

    // increment time so everything won't happen in the same second
    time_t now = rtc_get_time();
    rtc_set_time(now + 2);
  }

  // ensure the one that is evicted is INVALID_ID
  cl_assert_equal_i(DESIRED_EVICT_ID, app_cache_get_next_eviction());
}

void test_app_cache__clear(void) {
  // add all three
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app1.id, app1.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app2.id, app2.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app3.id, app3.size));

  app_cache_flush();

  cl_assert_equal_b(false, app_cache_entry_exists(app1.id));
  cl_assert_equal_b(false, app_cache_entry_exists(app2.id));
  cl_assert_equal_b(false, app_cache_entry_exists(app3.id));
}

#define APP_CACHE_FILE_NAME "appcache"
#define APP_CACHE_MAX_SIZE 4000

typedef struct PACKED {
  time_t    install_date;
  time_t    last_launch;
  uint32_t  total_size;
  uint16_t  launch_count;
} AppCacheEntry;

void test_app_cache__corrupt_key(void) {
  // add three cache entries
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app1.id, app1.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app2.id, app2.size));
  cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(app3.id, app3.size));


  // add one with a key value of length 3
  // Raw SettingsFile calls
  SettingsFile file;
  status_t rv = settings_file_open(&file, APP_CACHE_FILE_NAME, APP_CACHE_MAX_SIZE);

  cl_assert_equal_i(S_SUCCESS, rv);

  AppCacheEntry entry = {
    .install_date = rtc_get_time(),
    .last_launch = 0,
    .launch_count = 0,
    .total_size = 17,
  };

  int rand_id = 1717;
  rv = settings_file_set(&file, (uint8_t *)&rand_id, (sizeof(AppInstallId) - 1),
      (uint8_t *)&entry, sizeof(AppCacheEntry));

  settings_file_close(&file);
  // End Raw SettingsFile calls

  // force iteration on the app_cache. This will find the corrupted entry, and delete the app cache
  app_cache_free_up_space(1);

  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_b(false, app_cache_entry_exists(app1.id));
  cl_assert_equal_b(false, app_cache_entry_exists(app2.id));
  cl_assert_equal_b(false, app_cache_entry_exists(app3.id));
}

static const uint32_t SIZE_SUM = 7210515;
static const AppData t_data[] = {               // higher rank = should keep around
  { .id = 1,  .priority = 40, .size = 131798 }, // priority rank 28
  { .id = 2,  .priority = 60, .size = 194327 }, // priority rank 48
  { .id = 3,  .priority = 23, .size = 195131 }, // priority rank 15
  { .id = 4,  .priority = 21, .size = 16438  }, // priority rank 13
  { .id = 5,  .priority = 58, .size = 88644  }, // priority rank 45
  { .id = 6,  .priority = 57, .size = 269063 }, // priority rank 43
  { .id = 7,  .priority = 43, .size = 83456  }, // priority rank 32
  { .id = 8,  .priority = 29, .size = 233211 }, // priority rank 20
  { .id = 9,  .priority = 38, .size = 55766  }, // priority rank 26
  { .id = 10, .priority = 19, .size = 28359  }, // priority rank 12
  { .id = 11, .priority = 29, .size = 82909  }, // priority rank 21
  { .id = 12, .priority = 53, .size = 132316 }, // priority rank 41
  { .id = 13, .priority = 45, .size = 214356 }, // priority rank 35
  { .id = 14, .priority = 47, .size = 258908 }, // priority rank 36
  { .id = 15, .priority = 19, .size = 117885 }, // priority rank 11
  { .id = 16, .priority = 42, .size = 167427 }, // priority rank 31
  { .id = 17, .priority = 1,  .size = 22644  }, // priority rank 2
  { .id = 18, .priority = 30, .size = 33202  }, // priority rank 22
  { .id = 19, .priority = 25, .size = 151434 }, // priority rank 18
  { .id = 20, .priority = 33, .size = 102321 }, // priority rank 24
  { .id = 21, .priority = 19, .size = 223352 }, // priority rank 9
  { .id = 22, .priority = 36, .size = 133221 }, // priority rank 25
  { .id = 23, .priority = 51, .size = 169128 }, // priority rank 39
  { .id = 24, .priority = 22, .size = 103055 }, // priority rank 14
  { .id = 25, .priority = 44, .size = 182304 }, // priority rank 33
  { .id = 26, .priority = 2,  .size = 177430 }, // priority rank 3
  { .id = 27, .priority = 5,  .size = 248430 }, // priority rank 4
  { .id = 28, .priority = 44, .size = 168622 }, // priority rank 34
  { .id = 29, .priority = 6,  .size = 192857 }, // priority rank 5
  { .id = 30, .priority = 19, .size = 183331 }, // priority rank 10
  { .id = 31, .priority = 61, .size = 111155 }, // priority rank 50
  { .id = 32, .priority = 42, .size = 211695 }, // priority rank 30
  { .id = 33, .priority = 49, .size = 35653  }, // priority rank 38
  { .id = 34, .priority = 57, .size = 11541  }, // priority rank 44
  { .id = 35, .priority = 40, .size = 49368  }, // priority rank 29
  { .id = 36, .priority = 25, .size = 230982 }, // priority rank 17
  { .id = 37, .priority = 32, .size = 185018 }, // priority rank 23
  { .id = 38, .priority = 39, .size = 163897 }, // priority rank 27
  { .id = 39, .priority = 24, .size = 233217 }, // priority rank 16
  { .id = 40, .priority = 8,  .size = 23717  }, // priority rank 6
  { .id = 41, .priority = 61, .size = 266668 }, // priority rank 49
  { .id = 42, .priority = 58, .size = 61228  }, // priority rank 46
  { .id = 43, .priority = 12, .size = 23513  }, // priority rank 7
  { .id = 44, .priority = 60, .size = 267049 }, // priority rank 47
  { .id = 45, .priority = 52, .size = 240086 }, // priority rank 40
  { .id = 46, .priority = 14, .size = 194481 }, // priority rank 8
  { .id = 47, .priority = 27, .size = 42163  }, // priority rank 19
  { .id = 48, .priority = 56, .size = 72854  }, // priority rank 42
  { .id = 49, .priority = 49, .size = 217548 }, // priority rank 37
  { .id = 50, .priority = 1,  .size = 207357 }, // priority rank 1
};

extern uint32_t app_cache_get_size(void);

void prv_load_lotta_apps(void) {
  for (int i = 0; i < 50; i++) {
    // time is the basis of the priority. Set the time so we know what priority.
    rtc_set_time(t_data[i].priority);
    cl_assert_equal_i(S_SUCCESS, app_cache_add_entry(t_data[i].id, t_data[i].size));
    cl_assert_equal_i(S_SUCCESS, app_cache_app_launched(t_data[i].id));
    // increment time so everything won't happen in the same second
  }

  for (int i = 0; i < 50; i++) {
    cl_assert_equal_b(true, app_cache_entry_exists(t_data[i].id));
  }

  cl_assert_equal_i(SIZE_SUM, app_cache_get_size());
}

void prv_cleanup(void) {
  app_cache_flush();
  prv_load_lotta_apps();
}

void test_app_cache__free_up_space_lots_apps(void) {
  uint32_t to_free;
  uint32_t before_size;
  uint32_t after_size;

  // test random number
  prv_cleanup();
  to_free = 150000;
  before_size = app_cache_get_size();
  cl_assert(before_size >= to_free);
  cl_assert_equal_i(S_SUCCESS, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert(after_size <= (before_size - to_free));

  // test lowest priority's size
  prv_cleanup();
  to_free = 207357;
  before_size = app_cache_get_size();
  cl_assert(before_size >= to_free);
  cl_assert_equal_i(S_SUCCESS, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert_equal_i(after_size, (before_size - to_free));

  // test lowest priority's size
  prv_cleanup();
  to_free = 1; // should remove 207357
  before_size = app_cache_get_size();
  cl_assert(before_size >= to_free);
  cl_assert_equal_i(S_SUCCESS, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert_equal_i(after_size, (before_size - 207357));

  // test two lowest priority's size
  prv_cleanup();
  to_free = (207357 + 22644);
  before_size = app_cache_get_size();
  cl_assert(before_size >= to_free);
  cl_assert_equal_i(S_SUCCESS, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert_equal_i(after_size, (before_size - to_free));

  // test removing all binaries
  prv_cleanup();
  to_free = SIZE_SUM;
  before_size = app_cache_get_size();
  cl_assert(before_size >= to_free);
  cl_assert_equal_i(S_SUCCESS, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert_equal_i(after_size, 0);

  // test removing 0 bytes
  prv_cleanup();
  to_free = 0;
  before_size = app_cache_get_size();
  cl_assert(before_size >= to_free);
  cl_assert_equal_i(E_INVALID_ARGUMENT, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert_equal_i(after_size, before_size);

  // test removing all and more bytes
  prv_cleanup();
  to_free = SIZE_SUM + 1;
  before_size = app_cache_get_size();
  cl_assert(before_size < to_free);
  cl_assert_equal_i(S_SUCCESS, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert_equal_i(after_size, 0);
}

static bool prv_file_for_id_exists(AppInstallId id) {
  char buffer[30];
  itoa_int(id, buffer, 10);

  int fd = pfs_open(buffer, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  if (fd < 0) {
    return false;
  }
  pfs_close(fd);
  return true;
}

static void prv_create_file_for_id(AppInstallId id) {
  char buffer[30];
  itoa_int(id, buffer, 10);

  int fd = pfs_open(buffer, OP_FLAG_WRITE, FILE_TYPE_STATIC, 10);
  pfs_close(fd);
}

void test_app_cache__delete_binaries_for_id_with_no_entry(void) {
  // confirm binaries get created
  prv_create_file_for_id(17);
  cl_assert_equal_b(true, prv_file_for_id_exists(17));

  // confirm binaries are deleted
  app_cache_remove_entry(17);
  cl_assert_equal_b(false, prv_file_for_id_exists(17));
}

void test_app_cache__free_up_space_save_defaults(void) {
  uint32_t to_free;
  uint32_t before_size;
  uint32_t after_size;

  // lets save ids 17, 25, 42, and 47 because they are my favorite numbers
  uint32_t after_free = t_data[16].size + t_data[24].size + t_data[41].size + t_data[46].size;

  s_test_id_ql_up = 17;
  s_test_id_ql_down = 25;
  s_test_id_watchface = 42;
  s_test_id_worker = 47;

  // test removing all and more bytes
  prv_cleanup();
  to_free = SIZE_SUM - after_free;
  before_size = app_cache_get_size();
  PBL_LOG(LOG_LEVEL_DEBUG, "%d %d %d", to_free, after_free, before_size);
  cl_assert(before_size >= to_free);
  cl_assert_equal_i(S_SUCCESS, app_cache_free_up_space(to_free));
  fake_system_task_callbacks_invoke_pending();
  after_size = app_cache_get_size();
  cl_assert(after_size == (before_size - to_free));
}

struct file_description {
  const char *name;
  size_t size;
};

static struct file_description descriptions[] = {
  // this first set of files match some I found on my snowy bb2
  {"gap_bonding_db", 8102},
  {"pmap", 5632},
  {"pindb", 57095},
  {"appdb", 32603},
  {"reminderdb", 57090},
  {"appcache", 8108},
  {"alarms", 8110},
  {"notifpref", 8107},
  {"activity", 24436},
  {"insights", 8108},
  {"shellpref", 8107},
  {"dls_storage_33", 4096},
  {"dls_storage_122", 4096},
  {"dls_storage_84", 4096},
  {"dls_storage_71", 4096},
  {"dls_storage_107", 4096},
  {"dls_storage_176", 12555},
  {"dls_storage_161", 4096},
  {"dls_storage_110", 4096},
  {"dls_storage_142", 4096},
  {"dls_storage_197", 4096},
  {"dls_storage_218", 4096},
  {"dls_storage_145", 4096},
  {"app_comm", 8108},
  {"wakeup", 16274},
  {"notifstr", 30720},
  {"dls_storage_238", 4096},
  {"dls_storage_116", 4096},
  {"dls_storage_199", 4096},
  // throw in a few almost-but-not-quite look like resource files
  {"@0123ABCD/res", 1024},
  {"@01234567/ress", 1024},
  {"@01234567/re", 1024},
  {"!01234567/res", 1024}

};

static void prv_file_create(const char *name, size_t size) {
  int fd = pfs_open(name, OP_FLAG_WRITE, FILE_TYPE_STATIC, size);
  cl_assert(fd >= 0);
  pfs_close(fd);
}

static void prv_app_filename(char *filename, size_t size, int id) {
  app_file_name_make(filename, size, id, APP_FILE_NAME_SUFFIX, strlen(APP_FILE_NAME_SUFFIX));
}

static void prv_res_filename(char *filename, size_t size, int id) {
  app_file_name_make(filename, size, id, APP_RESOURCES_FILENAME_SUFFIX,
                     strlen(APP_RESOURCES_FILENAME_SUFFIX));
}

// create app and res files (and add to the app cache)
static void prv_app_files_create(int id) {
  char filename[15];
  // @xxxxxxxx/app
  prv_app_filename(filename, sizeof(filename), id);
  prv_file_create(filename, 64738);
  // @xxxxxxxx/res
  prv_res_filename(filename, sizeof(filename), id);
  prv_file_create(filename, 788);
  app_cache_add_entry(id, 64738);
}

static void prv_check_file_exists(const char *filename) {
  int fd = pfs_open(filename, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  cl_assert(fd >= 0);
  pfs_close(fd);
}

void test_app_cache__purge_orphaned_files(void) {
  // create some 'standard' files
  for (uint32_t i = 0; i < ARRAY_LENGTH(descriptions); ++i) {
    prv_file_create(descriptions[i].name, descriptions[i].size);
  }
  // create some app and res files and list them in the app cache
  for (uint32_t i = 0; i < 15; ++i) {
    prv_app_files_create((i + 1) * 257);
  }
  // create some res files that we expect to get purged
  prv_file_create("@00000000/res", 1024);
  prv_file_create("@00000001/res", 1024);
  prv_file_create("@ffffffff/res", 1024);
  // re-initialize the app-cache (which should purge the above 3 files)
  app_cache_init();
  // let's see if the three files we should have deleted are indeed gone
  cl_assert(pfs_open("@00000000/res", OP_FLAG_READ, FILE_TYPE_STATIC, 0) < 0);
  cl_assert(pfs_open("@00000001/res", OP_FLAG_READ, FILE_TYPE_STATIC, 0) < 0);
  cl_assert(pfs_open("@ffffffff/res", OP_FLAG_READ, FILE_TYPE_STATIC, 0) < 0);
  // let's make sure the app and res files in the cache weren't deleted
  for (uint32_t i = 0; i < 15; ++i) {
    char filename[15];
    int id = (i + 1) * 257;
    prv_app_filename(filename, sizeof(filename), id);
    prv_check_file_exists(filename);
    prv_res_filename(filename, sizeof(filename), id);
    prv_check_file_exists(filename);
  }
  // finally, make sure the 'standard' files are all there
  for (uint32_t i = 0; i < ARRAY_LENGTH(descriptions); ++i) {
    prv_check_file_exists(descriptions[i].name);
  }
}

void test_app_cache__purge_orphaned_files_no_apps(void) {
  // make sure nothing goes awry if there are no installed apps
  // create some 'standard' files
  for (uint32_t i = 0; i < ARRAY_LENGTH(descriptions); ++i) {
    prv_file_create(descriptions[i].name, descriptions[i].size);
  }
  // re-initialize the app-cache (which will attempt to purge resource files too)
  app_cache_init();
  // make sure the 'standard' files are all there
  for (uint32_t i = 0; i < ARRAY_LENGTH(descriptions); ++i) {
    prv_check_file_exists(descriptions[i].name);
  }
}
