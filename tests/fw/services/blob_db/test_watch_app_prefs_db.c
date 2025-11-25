/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/blob_db/watch_app_prefs_db.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/weather/weather_service_private.h"
#include "util/uuid.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_kernel_services_notifications.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

extern const char *PREF_KEY_SEND_TEXT_APP;
#define SEND_TEXT_KEY ((uint8_t *)PREF_KEY_SEND_TEXT_APP)
#define SEND_TEXT_KEY_LEN (strlen(PREF_KEY_SEND_TEXT_APP))

#define INVALID_KEY ((uint8_t *)"thisIsNotAnApp")
#define INVALID_KEY_LEN strlen((char *)INVALID_KEY)

#define NUM_SEND_TEXT_CONTACTS (5)
#define SEND_TEXT_DATA_LEN (sizeof(SerializedSendTextPrefs) + \
                           (sizeof(SerializedSendTextContact) * NUM_SEND_TEXT_CONTACTS))
static uint8_t s_send_text_prefs[SEND_TEXT_DATA_LEN];

#define WEATHER_KEY ((uint8_t *)PREF_KEY_WEATHER_APP)
#define WEATHER_KEY_LEN (strlen(PREF_KEY_WEATHER_APP))
#define NUM_WEATHER_LOCATIONS (4)
#define WEATHER_DATA_SIZE (sizeof(SerializedWeatherAppPrefs) + \
                          (sizeof(Uuid) * NUM_WEATHER_LOCATIONS))
static uint8_t s_weather_prefs[WEATHER_DATA_SIZE];

// Setup
////////////////////////////////////////////////////////////////

void test_watch_app_prefs_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  watch_app_prefs_db_init();

  // Set up our test prefs with some random data
  SerializedSendTextPrefs *prefs = (SerializedSendTextPrefs *)s_send_text_prefs;
  prefs->num_contacts = NUM_SEND_TEXT_CONTACTS;
  for (int i = 0; i < NUM_SEND_TEXT_CONTACTS; ++i) {
    prefs->contacts[i] = (SerializedSendTextContact) {
      .is_fav = (i < (NUM_SEND_TEXT_CONTACTS/2)),
    };
    uuid_generate(&prefs->contacts[i].contact_uuid);
    uuid_generate(&prefs->contacts[i].address_uuid);
  }

  SerializedWeatherAppPrefs *weather_prefs = (SerializedWeatherAppPrefs *)s_weather_prefs;
  weather_prefs->num_locations = NUM_WEATHER_LOCATIONS;
  for (int i = 0; i < NUM_WEATHER_LOCATIONS; i++) {
    uuid_generate(&weather_prefs->locations[i]);
  }
}

void test_watch_app_prefs_db__cleanup(void) {
}

// Tests
////////////////////////////////////////////////////////////////

void test_watch_app_prefs_db__insert_send_text(void) {
  const int data_len = sizeof(s_send_text_prefs);

  cl_assert_equal_i(
      watch_app_prefs_db_insert(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN, s_send_text_prefs, data_len),
      S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_get_len(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN), data_len);

  // Make sure we get back the correct data
  uint8_t send_text_prefs_out[data_len];
  cl_assert_equal_i(
      watch_app_prefs_db_read(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN, send_text_prefs_out, data_len),
      S_SUCCESS);
  cl_assert_equal_m(s_send_text_prefs, send_text_prefs_out, data_len);

  // Make sure we reject malformed data (not aligned to list size)
  cl_assert_equal_i(watch_app_prefs_db_insert(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN, s_send_text_prefs,
      (data_len + 1)), E_INVALID_ARGUMENT);

  // Make sure we reject data that is too small to hold all entries
  cl_assert_equal_i(watch_app_prefs_db_insert(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN, s_send_text_prefs,
      (data_len - sizeof(SerializedSendTextContact))), E_INVALID_ARGUMENT);

  // Make sure we reject keys we don't recognize
  cl_assert_equal_i(watch_app_prefs_db_insert(INVALID_KEY, INVALID_KEY_LEN, s_send_text_prefs,
      data_len), E_INVALID_ARGUMENT);
}

void test_watch_app_prefs_db__insert_weather(void) {
  const int data_len = sizeof(s_weather_prefs);

  cl_assert_equal_i(
      watch_app_prefs_db_insert(WEATHER_KEY, WEATHER_KEY_LEN, s_weather_prefs, data_len),
      S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_get_len(WEATHER_KEY, WEATHER_KEY_LEN), data_len);

  // Make sure we get back the correct data
  uint8_t weather_prefs_out[data_len];
  cl_assert_equal_i(
      watch_app_prefs_db_read(WEATHER_KEY, WEATHER_KEY_LEN, weather_prefs_out, data_len),
      S_SUCCESS);
  cl_assert_equal_m(s_weather_prefs, weather_prefs_out, data_len);

  // Make sure we reject malformed data (not aligned to list size)
  cl_assert_equal_i(watch_app_prefs_db_insert(WEATHER_KEY, WEATHER_KEY_LEN, s_weather_prefs,
      (data_len + 1)), E_INVALID_ARGUMENT);

  // Make sure we reject data that is too small to hold all entries
  cl_assert_equal_i(watch_app_prefs_db_insert(WEATHER_KEY, WEATHER_KEY_LEN, s_weather_prefs,
      (data_len - sizeof(SerializedWeatherAppPrefs))), E_INVALID_ARGUMENT);
}

void test_watch_app_prefs_db__insert_remove(void) {
  cl_assert_equal_i(watch_app_prefs_db_insert(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN, s_send_text_prefs,
                                              sizeof(s_send_text_prefs)), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_delete(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_get_len(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN), 0);

  cl_assert_equal_i(watch_app_prefs_db_insert(WEATHER_KEY, WEATHER_KEY_LEN, s_weather_prefs,
                                              sizeof(s_weather_prefs)), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_delete(WEATHER_KEY, WEATHER_KEY_LEN), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_get_len(WEATHER_KEY, WEATHER_KEY_LEN), 0);
}

void test_watch_app_prefs_db__flush(void) {
  cl_assert_equal_i(watch_app_prefs_db_insert(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN, s_send_text_prefs,
                                              sizeof(s_send_text_prefs)), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_insert(WEATHER_KEY, WEATHER_KEY_LEN, s_weather_prefs,
                                              sizeof(s_weather_prefs)), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_flush(), S_SUCCESS);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(watch_app_prefs_db_get_len(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN), 0);
}

void test_watch_app_prefs_db__get_send_text(void) {
  const int data_len = sizeof(s_send_text_prefs);

  cl_assert_equal_i(watch_app_prefs_db_insert(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN,
                                              s_send_text_prefs, data_len), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_get_len(SEND_TEXT_KEY, SEND_TEXT_KEY_LEN), data_len);

  SerializedSendTextPrefs *send_text_prefs_out = watch_app_prefs_get_send_text();
  cl_assert_equal_m(s_send_text_prefs, send_text_prefs_out, data_len);
  task_free(send_text_prefs_out);
}

void test_watch_app_prefs_db__get_weather(void) {
  const int data_len = sizeof(s_weather_prefs);

  cl_assert_equal_i(watch_app_prefs_db_insert(WEATHER_KEY, WEATHER_KEY_LEN,
                                              s_weather_prefs, data_len), S_SUCCESS);
  cl_assert_equal_i(watch_app_prefs_db_get_len(WEATHER_KEY, WEATHER_KEY_LEN), data_len);

  SerializedWeatherAppPrefs *weather_prefs_out = watch_app_prefs_get_weather();
  cl_assert_equal_m(s_weather_prefs, weather_prefs_out, data_len);
  watch_app_prefs_destroy_weather(weather_prefs_out);
}
