/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weather_data_shared.h"

#include <pbl/fs/fs.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/time.h>
#include <pbl/services/blob_db/weather_db.h>
#include <pbl/services/weather/weather_types.h>
#include <pbl/util/pstring.h>

#include <clar.h>

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include <pbl/kernel/malloc.h>

#include <fake_pbl_malloc.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>

// Stubs
////////////////////////////////////////////////////////////////

bool weather_service_supported_by_phone(void) {
  return true;
}
// Setup
////////////////////////////////////////////////////////////////

void test_weather_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  weather_db_init();
  weather_shared_data_init();
}

void test_weather_db__cleanup(void) {
  weather_shared_data_cleanup();
}

// Tests
////////////////////////////////////////////////////////////////
static void prv_db_iterator_cb(WeatherDBKey *key, WeatherDBEntry *entry, void *unused) {
  weather_shared_data_assert_entries_equal(
      key, entry, weather_shared_data_get_entry(weather_shared_data_get_index_of_key(key)));
}

void test_weather_db__get_entries(void) {
  cl_assert_equal_i(S_SUCCESS, weather_db_for_each(prv_db_iterator_cb, nullptr));
}

void test_weather_db__check_records_in_db(void) {
  for (int index = 0; index < WEATHER_DATA_SHARED_WEATHER_DB_NUM_DB_ENTRIES; index++) {
    WeatherDBEntry *to_check = pbl_zalloc_check(weather_shared_data_get_entry_size(index));
    const WeatherDBKey *key = weather_shared_data_get_key(index);
    cl_assert_equal_i(S_SUCCESS,
                      weather_db_read((uint8_t *)key, sizeof(WeatherDBKey), (uint8_t *)to_check,
                                      weather_shared_data_get_entry_size(index)));

    WeatherDBEntry *original = weather_shared_data_get_entry(index);
    weather_shared_data_assert_entries_equal(key, to_check, original);
    pbl_free(to_check);
  }
}

void test_weather_db__check_small_record_not_inserted(void) {
  const size_t entry_size = MIN_ENTRY_SIZE - 1;
  void *entry = pbl_zalloc_check(entry_size);
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};

  cl_assert_equal_i(E_INVALID_ARGUMENT, weather_db_insert((uint8_t *)&key, sizeof(WeatherDBKey),
                                                          (uint8_t *)entry, entry_size));
  pbl_free(entry);
}

void test_weather_db__check_too_large_record_not_inserted(void) {
  const size_t entry_size = MAX_ENTRY_SIZE + 1;
  void *entry = pbl_zalloc_check(entry_size);
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};

  cl_assert_equal_i(E_INVALID_ARGUMENT, weather_db_insert((uint8_t *)&key, sizeof(WeatherDBKey),
                                                          (uint8_t *)entry, entry_size));
  pbl_free(entry);
}

static void prv_check_invalid_version_code_not_inserted(uint8_t version) {
  const WeatherDBEntry *existing_entry = weather_shared_data_get_entry(0);
  const size_t entry_size = sizeof(*existing_entry);

  WeatherDBEntry *new_entry = pbl_zalloc_check(entry_size);
  *new_entry = *existing_entry;
  new_entry->version = version;

  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};
  cl_assert_equal_i(E_INVALID_ARGUMENT, weather_db_insert((uint8_t *)&key, sizeof(WeatherDBKey),
                                                          (uint8_t *)new_entry, entry_size));
  pbl_free(new_entry);
}

void test_weather_db__lower_version_not_inserted(void) {
  // v3 (WEATHER_DB_LEGACY_VERSION) stays insertable for the phone-app rollout window.
  for (size_t version = 0; version < WEATHER_DB_LEGACY_VERSION; version++) {
    prv_check_invalid_version_code_not_inserted(version);
  }
}

void test_weather_db__higher_version_not_inserted(void) {
  prv_check_invalid_version_code_not_inserted(WEATHER_DB_CURRENT_VERSION + 1);
}

// A record of a minor newer than the firmware knows: the current fixed part, `extra` bytes of
// fields it does not know, then the strings. `stated_size` is the fixed size the record states.
static uint8_t *prv_create_newer_minor_record(size_t extra, uint16_t stated_size,
                                              size_t *size_out) {
  const char *location = "OSLO";
  const char *phrase = "Snow";
  const size_t data_size = strlen(location) + strlen(phrase) + (sizeof(uint16_t) * 2);
  const size_t fixed_size = WEATHER_DB_V4_FIXED_SIZE + extra;
  const size_t size = fixed_size + sizeof(struct pbl_serialized_array) + data_size;
  uint8_t *record = pbl_zalloc_check(size);
  memcpy(record, weather_shared_data_get_entry(0), WEATHER_DB_V4_FIXED_SIZE);
  memset(record + WEATHER_DB_V4_FIXED_SIZE, 0xA5, extra);
  ((WeatherDBEntry *)record)->minor_version = WEATHER_DB_CURRENT_MINOR_VERSION + 1;
  ((WeatherDBEntry *)record)->fixed_size = stated_size;
  struct pbl_serialized_array *strings = (struct pbl_serialized_array *)(record + fixed_size);
  strings->data_size = data_size;
  weather_shared_data_write_strings(strings, location, phrase);
  *size_out = size;
  return record;
}

void test_weather_db__newer_minor_inserted(void) {
  // A newer minor appends fixed fields and states where they end: the strings are found there.
  const size_t extra = 24;
  size_t size;
  uint8_t *record = prv_create_newer_minor_record(extra, WEATHER_DB_V4_FIXED_SIZE + extra, &size);
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};
  cl_assert_equal_i(S_SUCCESS, weather_db_insert((uint8_t *)&key, sizeof(key), record, size));

  uint8_t *read = pbl_zalloc_check(size);
  cl_assert_equal_i(S_SUCCESS, weather_db_read((uint8_t *)&key, sizeof(key), read, size));
  struct pbl_pstring16_list strings;
  pbl_pstring16_list_init(&strings, weather_db_entry_get_strings((WeatherDBEntry *)read));
  cl_assert(pbl_pstring16_equal_cstring(
      pbl_pstring16_list_get(&strings, WeatherDbStringIndex_LocationName), "OSLO"));
  cl_assert(pbl_pstring16_equal_cstring(
      pbl_pstring16_list_get(&strings, WeatherDbStringIndex_ShortPhrase), "Snow"));

  cl_assert_equal_i(S_SUCCESS, weather_db_delete((uint8_t *)&key, sizeof(key)));
  pbl_free(read);
  pbl_free(record);
}

void test_weather_db__stated_size_below_known_fields_not_inserted(void) {
  // Well-formed strings that start inside the hourly series a minor 6 reader takes. The location
  // name makes the record as long as a whole minor 6 fixed part.
  const size_t stated_size = WEATHER_DB_V4_FIXED_SIZE - 2 * WEATHER_DB_HOURLY_COUNT;
  char location[2 * WEATHER_DB_HOURLY_COUNT + 1];
  memset(location, 'A', sizeof(location) - 1);
  location[sizeof(location) - 1] = '\0';
  const char *phrase = "Snow";
  const size_t data_size = strlen(location) + strlen(phrase) + (sizeof(uint16_t) * 2);
  const size_t size = stated_size + sizeof(struct pbl_serialized_array) + data_size;
  cl_assert(size >= WEATHER_DB_V4_FIXED_SIZE);
  uint8_t *record = pbl_zalloc_check(size);
  memcpy(record, weather_shared_data_get_entry(0), stated_size);
  ((WeatherDBEntry *)record)->fixed_size = stated_size;
  struct pbl_serialized_array *strings = (struct pbl_serialized_array *)(record + stated_size);
  strings->data_size = data_size;
  weather_shared_data_write_strings(strings, location, phrase);

  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    weather_db_insert((uint8_t *)&key, sizeof(key), record, size));
  pbl_free(record);
}

void test_weather_db__stated_size_past_record_end_not_inserted(void) {
  size_t size;
  uint8_t *record = prv_create_newer_minor_record(0, WEATHER_DB_V4_FIXED_SIZE, &size);
  // Where the strings would start is the end of the record
  ((WeatherDBEntry *)record)->fixed_size = size;
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    weather_db_insert((uint8_t *)&key, sizeof(key), record, size));
  pbl_free(record);
}

status_t weather_db_insert_stale(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

static void prv_count_entries_cb(WeatherDBKey *key, WeatherDBEntry *entry, void *context) {
  (*(int *)context)++;
}

void test_weather_db__stored_record_with_a_bad_stated_size_is_not_handed_out(void) {
  // Stored without insert's checks, stating a fixed size past its own end
  size_t size;
  uint8_t *record = prv_create_newer_minor_record(0, UINT16_MAX, &size);
  WeatherDBKey key = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
  cl_assert_equal_i(S_SUCCESS, weather_db_insert_stale((uint8_t *)&key, sizeof(key), record, size));

  int count = 0;
  cl_assert_equal_i(S_SUCCESS, weather_db_for_each(prv_count_entries_cb, &count));
  cl_assert_equal_i(count, WEATHER_DATA_SHARED_WEATHER_DB_NUM_DB_ENTRIES);

  uint8_t *buf = pbl_zalloc_check(size);
  cl_assert_equal_i(E_DOES_NOT_EXIST, weather_db_read((uint8_t *)&key, sizeof(key), buf, size));
  cl_assert_equal_i(0, weather_db_get_len((uint8_t *)&key, sizeof(key)));
  pbl_free(buf);
  pbl_free(record);
}

void test_weather_db__largest_record_inserted_and_one_byte_more_is_not(void) {
  // Fields this firmware does not know fill the record up to the limit
  const size_t strings_size = sizeof(struct pbl_serialized_array) + strlen("OSLO") +
                              strlen("Snow") + (sizeof(uint16_t) * 2);
  const size_t extra = MAX_ENTRY_SIZE - WEATHER_DB_V4_FIXED_SIZE - strings_size;
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};
  size_t size;
  uint8_t *record = prv_create_newer_minor_record(extra, WEATHER_DB_V4_FIXED_SIZE + extra, &size);
  cl_assert_equal_i(size, MAX_ENTRY_SIZE);
  cl_assert_equal_i(S_SUCCESS, weather_db_insert((uint8_t *)&key, sizeof(key), record, size));
  cl_assert_equal_i(S_SUCCESS, weather_db_delete((uint8_t *)&key, sizeof(key)));
  pbl_free(record);

  record = prv_create_newer_minor_record(extra + 1, WEATHER_DB_V4_FIXED_SIZE + extra + 1, &size);
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    weather_db_insert((uint8_t *)&key, sizeof(key), record, size));
  pbl_free(record);
}

void test_weather_db__sized_minor_shorter_than_its_size_field_not_inserted(void) {
  // Too short to hold the size a minor 6 record states
  const size_t size = WEATHER_DB_V4_0_FIXED_SIZE;
  uint8_t *record = pbl_zalloc_check(size);
  memcpy(record, weather_shared_data_get_entry(0), size);
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    weather_db_insert((uint8_t *)&key, sizeof(key), record, size));
  pbl_free(record);
}

void test_weather_db__legacy_v3_inserted(void) {
  // A well-formed legacy v3 record (v3 fixed prefix + trailing pstrings) must
  // still be accepted during the phone-app rollout window.
  const char *location = "SEATTLE";
  const char *phrase = "Rainy";
  const size_t data_size = strlen(location) + strlen(phrase) + (sizeof(uint16_t) * 2);
  const size_t entry_size = sizeof(WeatherDBEntryV3) + data_size;

  WeatherDBEntryV3 *entry = pbl_zalloc_check(entry_size);
  *entry = (WeatherDBEntryV3){
    .version = WEATHER_DB_LEGACY_VERSION,
    .is_current_location = true,
    .current_temp = 55,
    .current_weather_type = WeatherType_LightRain,
    .today_high_temp = 58,
    .today_low_temp = 48,
    .tomorrow_weather_type = WeatherType_CloudyDay,
    .tomorrow_high_temp = 60,
    .tomorrow_low_temp = 50,
    .last_update_time_utc = pbl_realtime(),
  };
  entry->pstring16s.data_size = data_size;

  weather_shared_data_write_strings(&entry->pstring16s, location, phrase);

  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6};
  cl_assert_equal_i(S_SUCCESS, weather_db_insert((uint8_t *)&key, sizeof(WeatherDBKey),
                                                 (uint8_t *)entry, entry_size));
  cl_assert_equal_i(S_SUCCESS, weather_db_delete((uint8_t *)&key, sizeof(WeatherDBKey)));
  pbl_free(entry);
}

void test_weather_db__v4_6_layout(void) {
  // Minor 6 appends the fixed size and four hourly series after minor 5's fields. The absolute
  // offsets are checked in weather_db.c, as this host build lays enums out wider than the watch
  // does.
  const size_t minor5_end =
      offsetof(WeatherDBEntry, tomorrow_hourly_temp) + WEATHER_DB_HOURLY_COUNT;
  cl_assert_equal_i(minor5_end, WEATHER_DB_V4_5_FIXED_SIZE);
  cl_assert_equal_i(minor5_end, offsetof(WeatherDBEntry, fixed_size));
  const size_t series = minor5_end + sizeof(uint16_t);
  cl_assert_equal_i(series, offsetof(WeatherDBEntry, today_hourly_precip_probability));
  cl_assert_equal_i(series + WEATHER_DB_HOURLY_COUNT,
                    offsetof(WeatherDBEntry, tomorrow_hourly_precip_probability));
  cl_assert_equal_i(series + 2 * WEATHER_DB_HOURLY_COUNT,
                    offsetof(WeatherDBEntry, today_hourly_precip_mm_x10));
  cl_assert_equal_i(series + 3 * WEATHER_DB_HOURLY_COUNT,
                    offsetof(WeatherDBEntry, tomorrow_hourly_precip_mm_x10));
  cl_assert_equal_i(series + 4 * WEATHER_DB_HOURLY_COUNT, WEATHER_DB_V4_FIXED_SIZE);
  cl_assert_equal_i(WEATHER_DB_V4_5_FIXED_SIZE,
                    weather_db_entry_strings_offset(WEATHER_DB_CURRENT_VERSION, 5));
  cl_assert_equal_i(WEATHER_DB_V4_FIXED_SIZE,
                    weather_db_entry_strings_offset(WEATHER_DB_CURRENT_VERSION, 6));
}

// A v4 record cut down to an older minor's fixed fields, `fixed_size` bytes, followed by its
// strings and stamped `minor`.
static uint8_t *prv_create_minor_record(uint8_t minor, size_t fixed_size, size_t *size_out) {
  const char *location = "OSLO";
  const char *phrase = "Snow";
  const size_t data_size = strlen(location) + strlen(phrase) + (sizeof(uint16_t) * 2);
  const size_t size = fixed_size + sizeof(struct pbl_serialized_array) + data_size;
  uint8_t *record = pbl_zalloc_check(size);
  memcpy(record, weather_shared_data_get_entry(0), fixed_size);
  ((WeatherDBEntry *)record)->minor_version = minor;
  struct pbl_serialized_array *strings = (struct pbl_serialized_array *)(record + fixed_size);
  strings->data_size = data_size;
  weather_shared_data_write_strings(strings, location, phrase);
  *size_out = size;
  return record;
}

void test_weather_db__older_minor_inserted(void) {
  // A minor 5 record's strings follow tomorrow's hourly series.
  size_t size;
  uint8_t *record = prv_create_minor_record(5, WEATHER_DB_V4_5_FIXED_SIZE, &size);
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7};
  cl_assert_equal_i(S_SUCCESS, weather_db_insert((uint8_t *)&key, sizeof(key), record, size));

  uint8_t *read = pbl_zalloc_check(size);
  cl_assert_equal_i(S_SUCCESS, weather_db_read((uint8_t *)&key, sizeof(key), read, size));
  struct pbl_pstring16_list strings;
  pbl_pstring16_list_init(&strings, weather_db_entry_get_strings((WeatherDBEntry *)read));
  cl_assert(pbl_pstring16_equal_cstring(
      pbl_pstring16_list_get(&strings, WeatherDbStringIndex_LocationName), "OSLO"));
  cl_assert(pbl_pstring16_equal_cstring(
      pbl_pstring16_list_get(&strings, WeatherDbStringIndex_ShortPhrase), "Snow"));

  cl_assert_equal_i(S_SUCCESS, weather_db_delete((uint8_t *)&key, sizeof(key)));
  pbl_free(read);
  pbl_free(record);
}

void test_weather_db__minor_without_its_fields_not_inserted(void) {
  // Stamped minor 6 but only as long as minor 5: its strings would start past the end.
  size_t size;
  uint8_t *record = prv_create_minor_record(6, WEATHER_DB_V4_5_FIXED_SIZE, &size);
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7};
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    weather_db_insert((uint8_t *)&key, sizeof(key), record, size));
  pbl_free(record);
}

// Inserts entry 0's first `fixed_size` bytes, stamped `version` and `minor`, then a string block
// header declaring `data_size` followed by `data_len` bytes of `data`. Returns the insert status
// and deletes the record if it was inserted.
static status_t prv_insert_with_strings(uint8_t version, uint8_t minor, size_t fixed_size,
                                        const uint8_t *data, size_t data_len, uint16_t data_size) {
  const size_t size = fixed_size + sizeof(struct pbl_serialized_array) + data_len;
  uint8_t *record = pbl_zalloc_check(size);
  memcpy(record, weather_shared_data_get_entry(0), fixed_size);
  ((WeatherDBEntry *)record)->version = version;
  if (version >= WEATHER_DB_CURRENT_VERSION) {
    ((WeatherDBEntry *)record)->minor_version = minor;
  }
  struct pbl_serialized_array *strings = (struct pbl_serialized_array *)(record + fixed_size);
  strings->data_size = data_size;
  memcpy(strings->data, data, data_len);
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8};
  const status_t rv = weather_db_insert((uint8_t *)&key, sizeof(key), record, size);
  if (rv == S_SUCCESS) {
    weather_db_delete((uint8_t *)&key, sizeof(key));
  }
  pbl_free(record);
  return rv;
}

void test_weather_db__strings_block_past_record_end_not_inserted(void) {
  // The header claims one byte more than the record holds.
  const uint8_t data[] = {4, 0, 'O', 'S', 'L', 'O', 4, 0, 'S', 'n', 'o', 'w'};
  cl_assert_equal_i(
      E_INVALID_ARGUMENT,
      prv_insert_with_strings(WEATHER_DB_CURRENT_VERSION, WEATHER_DB_CURRENT_MINOR_VERSION,
                              WEATHER_DB_V4_FIXED_SIZE, data, sizeof(data), sizeof(data) + 1));
}

void test_weather_db__pstring_past_block_end_not_inserted(void) {
  // The first string claims 200 bytes of a 6-byte block.
  const uint8_t data[] = {200, 0, 'O', 'S', 'L', 'O'};
  cl_assert_equal_i(
      E_INVALID_ARGUMENT,
      prv_insert_with_strings(WEATHER_DB_CURRENT_VERSION, WEATHER_DB_CURRENT_MINOR_VERSION,
                              WEATHER_DB_V4_FIXED_SIZE, data, sizeof(data), sizeof(data)));
}

void test_weather_db__pstring_length_over_255_walked_in_full(void) {
  // A legacy record with a 400-byte string block: a 256-byte first string, then a second whose
  // length cannot fit. Stepping by the low byte of the first length (0) would land on two more
  // strings that exactly fill the block and accept the record.
  const size_t block = 400;
  uint8_t *data = pbl_zalloc_check(block);
  data[0] = 0x00;
  data[1] = 0x01; // 256
  data[2] = 0xFF;
  data[3] = 0x00; // read as a 255-byte string by the low-byte walk, ending at 259
  data[258] = 0xFF;
  data[259] = (uint8_t)(block - 259 - sizeof(uint16_t)); // the rest of the block, to that walk
  cl_assert_equal_i(
      E_INVALID_ARGUMENT,
      prv_insert_with_strings(WEATHER_DB_LEGACY_VERSION, 0, offsetof(WeatherDBEntryV3, pstring16s),
                              data, block, block));
  pbl_free(data);
}

void test_weather_db__fixed_fields_only_not_inserted(void) {
  // Nothing after the fixed fields: not even the string block's header.
  const size_t size = WEATHER_DB_V4_FIXED_SIZE;
  uint8_t *record = pbl_zalloc_check(size);
  memcpy(record, weather_shared_data_get_entry(0), size);
  ((WeatherDBEntry *)record)->minor_version = WEATHER_DB_CURRENT_MINOR_VERSION;
  WeatherDBKey key = (WeatherDBKey){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8};
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    weather_db_insert((uint8_t *)&key, sizeof(key), record, size));
  pbl_free(record);
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

  for (int x = 0; x < WEATHER_DATA_SHARED_WEATHER_DB_NUM_DB_ENTRIES; x++) {
    cl_assert(weather_shared_data_get_key_exists(&keys[x]));
  }
}

void test_weather_db__read_stale_entries(void) {
  WeatherDBKey key = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  size_t entry_size = weather_shared_data_insert_stale_entry(&key);
  uint8_t *buf = pbl_zalloc_check(entry_size);

  cl_assert_equal_i(E_DOES_NOT_EXIST,
                    weather_db_read((uint8_t *)&key, sizeof(WeatherDBKey), buf, entry_size));
}
