/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/attributes.h"
#include "util/pstring.h"

#include "services/normal/blob_db/weather_db.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/weather/weather_types.h"
#include "weather_data_shared.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_pbl_malloc.h"
#include "fake_spi_flash.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_prompt.h"
#include "stubs_task_watchdog.h"
#include "stubs_pebble_tasks.h"
#include "stubs_sleep.h"

bool weather_service_supported_by_phone(void) {
  return true;
}
// Setup
////////////////////////////////////////////////////////////////

void test_weather_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  weather_db_init();
  weather_shared_data_init();
}

void test_weather_db__cleanup(void) {
  weather_shared_data_cleanup();
}

// Tests
////////////////////////////////////////////////////////////////
static void prv_db_iterator_cb(WeatherDBKey *key, WeatherDBEntry *entry, void *unused) {
  weather_shared_data_assert_entries_equal(key, entry,
      weather_shared_data_get_entry(weather_shared_data_get_index_of_key(key)));
}

void test_weather_db__get_entries(void) {
  cl_assert_equal_i(S_SUCCESS, weather_db_for_each(prv_db_iterator_cb, NULL));
}

void test_weather_db__check_records_in_db(void) {
  for (int index = 0; index < WEATHER_DATA_SHARED_WEATHER_DB_NUM_DB_ENTRIES; index++) {
    WeatherDBEntry *to_check = task_zalloc_check(weather_shared_data_get_entry_size(index));
    const WeatherDBKey *key = weather_shared_data_get_key(index);
    cl_assert_equal_i(S_SUCCESS, weather_db_read((uint8_t*)key,
                                                 sizeof(WeatherDBKey),
                                                 (uint8_t*)to_check,
                                                 weather_shared_data_get_entry_size(index)));

    WeatherDBEntry *original = weather_shared_data_get_entry(index);
    weather_shared_data_assert_entries_equal(key, to_check, original);
    task_free(to_check);
  }
}

void test_weather_db__check_small_record_not_inserted(void) {
  const size_t entry_size = MIN_ENTRY_SIZE - 1;
  void *entry = task_zalloc_check(entry_size);
  WeatherDBKey key = (WeatherDBKey) {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5
  };

  cl_assert_equal_i(E_INVALID_ARGUMENT, weather_db_insert((uint8_t*)&key,
                                                          sizeof(WeatherDBKey),
                                                          (uint8_t*)entry,
                                                          entry_size));
  task_free(entry);
}

void test_weather_db__check_too_large_record_not_inserted(void) {
  const size_t entry_size = MAX_ENTRY_SIZE + 1;
  void *entry = task_zalloc_check(entry_size);
  WeatherDBKey key = (WeatherDBKey) {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5
  };

  cl_assert_equal_i(E_INVALID_ARGUMENT, weather_db_insert((uint8_t*)&key,
                                                          sizeof(WeatherDBKey),
                                                          (uint8_t*)entry,
                                                          entry_size));
  task_free(entry);
}

static void prv_check_invalid_version_code_not_inserted(uint8_t version) {
  const WeatherDBEntry *existing_entry = weather_shared_data_get_entry(0);
  const size_t entry_size = sizeof(*existing_entry);

  WeatherDBEntry *new_entry = task_zalloc_check(entry_size);
  *new_entry = *existing_entry;
  new_entry->version = version;

  WeatherDBKey key = (WeatherDBKey) {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5
  };
  cl_assert_equal_i(E_INVALID_ARGUMENT, weather_db_insert((uint8_t*)&key,
                                                          sizeof(WeatherDBKey),
                                                          (uint8_t*)new_entry,
                                                          entry_size));
  task_free(new_entry);
}

void test_weather_db__lower_version_not_inserted(void) {
  for (size_t version = 0; version < WEATHER_DB_CURRENT_VERSION; version++) {
    prv_check_invalid_version_code_not_inserted(version);
  }
}

void test_weather_db__higher_version_not_inserted(void) {
  prv_check_invalid_version_code_not_inserted(WEATHER_DB_CURRENT_VERSION + 1);
}

status_t weather_db_get_num_keys(uint16_t *val_out);

void test_weather_db__test_get_num_keys(void) {
  uint16_t num_keys;
  cl_assert_equal_i(S_SUCCESS, weather_db_get_num_keys(&num_keys));
  cl_assert_equal_i(num_keys, WEATHER_DATA_SHARED_WEATHER_DB_NUM_DB_ENTRIES);
}

status_t weather_db_get_keys(WeatherDBKey *keys);

void test_weather_db__test_get_keys(void) {
  WeatherDBKey keys[WEATHER_DATA_SHARED_WEATHER_DB_NUM_DB_ENTRIES];
  cl_assert_equal_i(S_SUCCESS, weather_db_get_keys(keys));

  for(int x = 0; x < WEATHER_DATA_SHARED_WEATHER_DB_NUM_DB_ENTRIES; x++) {
    cl_assert(weather_shared_data_get_key_exists(&keys[x]));
  }
}

void test_weather_db__read_stale_entries(void) {
  WeatherDBKey key = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
  size_t entry_size = weather_shared_data_insert_stale_entry(&key);
  uint8_t *buf = task_zalloc_check(entry_size);

  cl_assert_equal_i(E_DOES_NOT_EXIST, weather_db_read((uint8_t*)&key,
                                                      sizeof(WeatherDBKey),
                                                      buf,
                                                      entry_size));
}
