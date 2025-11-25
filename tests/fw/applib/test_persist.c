/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <stdio.h>
#include <string.h>

#include "applib/persist.h"
#include "flash_region/flash_region.h"
#include "process_management/app_install_manager.h"
#include "process_management/pebble_process_md.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/persist.h"
#include "system/logging.h"

// Stubs
////////////////////////////////////
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_system_reset.h"
#include "stubs_task_watchdog.h"

static PebbleProcessMd __pbl_app_info;

const PebbleProcessMd* sys_process_manager_get_current_process_md(void) {
  return &__pbl_app_info;
}

// Tests
////////////////////////////////////
#define TEST_UUID_A { 0x2F, 0xF7, 0xFA, 0x04, 0x60, 0x11, 0x4A, 0x98, 0x8A, 0x3B, 0xA8, 0x26, 0xA4, 0xB8, 0x99, 0xF8 }

static const int system_uuid_id = 0;
static const Uuid system_uuid = UUID_SYSTEM;

static const int test_uuid_a_id = 1;
static const Uuid test_uuid_a = TEST_UUID_A;

static const int test_uuid_b_id = 2;
static const Uuid test_uuid_b = { 0xC3, 0x0D, 0xBA, 0xF1, 0x5F, 0x6F, 0x4F, 0x22, 0xBA, 0xAA, 0x8C, 0x2A, 0x96, 0x8C, 0xFC, 0x28 };

static const int test_uuid_c_id = 3;
static const Uuid test_uuid_c = { 0x1D, 0x6C, 0x7F, 0x01, 0xD9, 0x48, 0x42, 0xA6, 0xAA, 0x4E, 0xB2, 0x08, 0x42, 0x10, 0xEB, 0xBC };

static PebbleProcessMd __pbl_app_info = {
  .uuid = TEST_UUID_A,
};

const char lipsum[] = "Lorem ipsum dolor sit amet, consectetur "
  "adipiscing elit. Nam dignissim ullamcorper sollicitudin. Suspendisse at "
  "urna suscipit, congue purus a, posuere eros. Nulla eros urna, vestibulum "
  "a dictum a, maximus sed nibh. Ut ut dui finibus, tincidunt ligula quis, "
  "ornare mi. Pellentesque sagittis suscipit lacus nec consectetur. Nunc et "
  "commodo neque. Vestibulum vitae dignissim sapien. Nulla scelerisque "
  "finibus nisl. Suspendisse ac massa lacus. In hac habitasse platea "
  "dictumst. Ut condimentum urna eros. Fusce ipsum metus, vehicula eu tortor "
  "sed, congue tempus mauris. Maecenas mollis lacus non cursus bibendum. "
  "Etiam id dolor lorem. Aenean scelerisque nulla sed tristique posuere. "
  "Proin dui magna, gravida faucibus ultricies non, tincidunt id metus. "
  "Integer a laoreet dolor, eu vulputate enim. Ut vitae hendrerit nunc, in "
  "bibendum eros. Pellentesque congue ut quam id sollicitudin. Cras "
  "malesuada arcu nec imperdiet cursus. Donec vitae ex eget mi imperdiet "
  "efficitur id eu velit. Proin pretium ipsum sed convallis efficitur. Morbi "
  "non feugiat erat. Ut ut efficitur massa. Sed eu auctor felis. Vestibulum "
  "magna orci, placerat nec risus nec, ultricies congue ex. Morbi in "
  "vestibulum leo. Nullam non dapibus lorem. Suspendisse blandit diam "
  "posuere suscipit malesuada. Maecenas vehicula felis eu posuere euismod. "
  "Fusce at velit ultrices, sagittis enim ac, ultrices lorem. Quisque "
  "tincidunt fringilla suscipit. Curabitur tempus lorem metus, sed venenatis "
  "augue maximus a. Duis venenatis tortor sit amet justo sodales suscipit. "
  "Morbi tincidunt rutrum nisl, eget placerat nisi condimentum a. Vestibulum "
  "ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia "
  "Curae; Cras varius sagittis mauris, in consequat sapien tincidunt vitae. "
  "Duis ipsum nunc, tristique sit amet blandit non, scelerisque non diam. "
  "Etiam condimentum aliquam dictum. Nam nisi ex, cursus in ligula sit amet, "
  "ultricies egestas libero. Aliquam luctus, metus quis ultricies sagittis, "
  "nisi orci viverra felis, vitae luctus massa dolor sit amet dolor. Cras "
  "mattis velit vitae pretium pulvinar. Pellentesque auctor, turpis at cras "
  "amet.";
_Static_assert(sizeof(lipsum) > PERSIST_STRING_MAX_LENGTH,
               "lipsum string is not long enough for persist tests");

void test_persist__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);

  pfs_init(false);
  persist_service_init();

  persist_service_client_open(&test_uuid_a);
}

void test_persist__cleanup(void) {
  persist_service_client_close(&test_uuid_a);
}

void test_persist__int(void) {
  const uint32_t key = 0;
  const uint32_t value = ~0;
  cl_assert_equal_i(persist_read_int(key), 0);
  cl_assert_equal_i(persist_write_int(key, value), sizeof(int));
  cl_assert_equal_i(persist_get_size(key), sizeof(int));
  cl_assert_equal_i(persist_read_int(key), value);
}

void test_persist__bool(void) {
  const uint32_t key = 0;
  cl_assert_equal_i(persist_read_bool(key), false);
  cl_assert_equal_i(persist_write_bool(key, true), sizeof(bool));
  cl_assert_equal_i(persist_get_size(key), sizeof(bool));
  cl_assert_equal_i(persist_read_bool(key), true);
}

void test_persist__data(void) {
  const uint32_t key = 0;
  const int size = sizeof(test_uuid_a);
  Uuid uuid_buffer;
  cl_assert_equal_i(persist_read_data(key, &uuid_buffer, sizeof(uuid_buffer)), E_DOES_NOT_EXIST);

  cl_assert_equal_i(persist_write_data(key, &test_uuid_a, sizeof(test_uuid_a)), size);

  cl_assert_equal_i(persist_get_size(key), size);

  cl_assert_equal_i(persist_read_data(key, &uuid_buffer, sizeof(uuid_buffer)), size);

  cl_assert(uuid_equal(&test_uuid_a, &uuid_buffer));
}

void test_persist__data_too_big(void) {
  char buf[PERSIST_DATA_MAX_LENGTH+2];
  memset(buf, '~', sizeof(buf));

  cl_assert_equal_i(persist_write_data(0, lipsum, sizeof(lipsum)),
                    PERSIST_DATA_MAX_LENGTH);
  cl_assert_equal_i(persist_read_data(0, buf, sizeof(buf)),
                    PERSIST_DATA_MAX_LENGTH);
  cl_assert(memcmp(lipsum, buf, PERSIST_DATA_MAX_LENGTH) == 0);
  for (size_t i = PERSIST_DATA_MAX_LENGTH; i < sizeof(buf); ++i) {
    cl_assert_(buf[i] == '~',
               "persist_read_data writes past the end of destination buffer");
  }
}

void test_persist__string_does_not_exist(void) {
  char string_buffer[PERSIST_STRING_MAX_LENGTH];
  memset(string_buffer, '~', sizeof(string_buffer));

  cl_assert_equal_i(
      persist_read_string(0, string_buffer, sizeof(string_buffer)),
      E_DOES_NOT_EXIST);

  for (size_t i = 0; i < sizeof(string_buffer); ++i) {
    if (string_buffer[i] != '~') {
      char error_msg[132];
      snprintf(error_msg, sizeof(error_msg), "persist_read_string clobbers "
               "destination buffer at %zd when key does not exist", i);
      cl_fail(error_msg);
    }
  }
}

void test_persist__string_write_unterminated_string(void) {
  char string_buffer[PERSIST_STRING_MAX_LENGTH + 2];
  memset(string_buffer, '~', sizeof(string_buffer));

  cl_assert_equal_i(persist_write_string(0, lipsum), PERSIST_STRING_MAX_LENGTH);
  cl_assert_equal_i(persist_get_size(0), PERSIST_STRING_MAX_LENGTH);

  cl_assert_equal_i(
      persist_read_string(0, string_buffer, sizeof(string_buffer)),
      PERSIST_STRING_MAX_LENGTH);

  cl_assert_equal_i(string_buffer[PERSIST_STRING_MAX_LENGTH - 1], '\0');
  cl_assert(strncmp(lipsum, string_buffer, PERSIST_STRING_MAX_LENGTH - 1) == 0);

  for (size_t i = PERSIST_STRING_MAX_LENGTH; i < sizeof(string_buffer); ++i) {
    cl_assert_(string_buffer[i] == '~',
               "persist_read_string writes past the end of destination buffer");
  }
}

void test_persist__size_of_nonexistent_key(void) {
  cl_assert_equal_i(persist_get_size(0), E_DOES_NOT_EXIST);
}

void test_persist__size(void) {
  char data[] = { 1, 2, 3, 4, 5, 6 };
  cl_assert_equal_i(persist_write_data(0, data, sizeof(data)), sizeof(data));
  cl_assert_equal_i(persist_get_size(0), sizeof(data));
}

void test_persist__exists(void) {
  cl_assert_equal_i(persist_exists(0), S_FALSE);
  cl_assert(PASSED(persist_write_int(0, 0)));
  cl_assert_equal_i(persist_exists(0), S_TRUE);
}

void test_persist__delete(void) {
  cl_assert_equal_i(persist_delete(0), E_DOES_NOT_EXIST);
  cl_assert(PASSED(persist_write_int(0, 0)));
  cl_assert_equal_i(persist_delete(0), S_TRUE);
  cl_assert_equal_i(persist_delete(0), E_DOES_NOT_EXIST);
}

/*
 * Confirm that fields can be reassigned values.
 */
void test_persist__overwrite(void) {
  const uint32_t key = 0;
  cl_assert_equal_i(persist_write_int(key, 1), sizeof(int));
  cl_assert_equal_i(persist_read_int(key), 1);
  cl_assert_equal_i(persist_write_int(key, 2), sizeof(int));
  cl_assert_equal_i(persist_read_int(key), 2);
}

/*
 * Confirm that overwriting with a smaller data size does not break tuple finding.
 */
void test_persist__overwrite_shrink(void) {
  cl_assert(PASSED(persist_write_int(0, 1)));
  cl_assert(PASSED(persist_write_bool(0, false)));
  cl_assert(PASSED(persist_write_int(1, 2)));
  cl_assert(persist_read_int(1) == 2);
}

/*
 * Confirm that loading a smaller amount of data, then a larger amount of data,
 * always returns the appropriate amount of data.
 */
void test_persist__partial_read_extension(void) {
  char buffer[] = "Hello thar";

  // Write out data
  cl_assert_equal_i(persist_write_string(0, buffer), sizeof(buffer));
  // Clear the cache (which has the entire string right now)
  persist_service_client_close(&test_uuid_a);
  persist_service_client_open(&test_uuid_a);
  // Reset out buffer so we can check the read's honesty.
  memset(buffer, 0, strlen(buffer));
  // Read part of the data we wrote
  cl_assert_equal_i(persist_read_string(0, buffer, 2), 2);
  cl_assert_equal_i(strlen(buffer), 2 - 1); // -1 because null termination
  cl_assert_equal_i(buffer[0], 'H');
  // Then attempt to read back the entire thing
  cl_assert_equal_i(persist_read_string(0, buffer, sizeof(buffer)), sizeof(buffer));
  cl_assert_equal_i(strlen(buffer), 10);
  cl_assert(strcmp(buffer, "Hello thar") == 0);
}

void test_persist__legacy2_max_usage(void) {
  uint8_t buffer[256];
  memset(buffer, 1, sizeof(buffer));

  // The maximum amount of 'buffer' sized fields allowed by the old persist
  // storage backend.
  int n = (4 * 1024) / (9 + sizeof(buffer));

  PBL_LOG_VERBOSE("n = %d", n);
  for (int i = 0; i < n; ++i) {
    PBL_LOG(LOG_LEVEL_DEBUG, "i = %d", i);
    cl_assert_equal_i(persist_write_data(i, &buffer, sizeof(buffer)), sizeof(buffer));
  }

  // Don't be too strict about preventing apps from using more persist than they
  // had available under the old implementation.
  // cl_assert_equal_i(persist_write_data(n + 1, &buffer, sizeof(buffer)),
  //                  E_OUT_OF_STORAGE);
}
