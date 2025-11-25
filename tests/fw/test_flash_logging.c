/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "flash_region/flash_region.h"
#include "debug/flash_logging.h"
#include "util/math.h"
#include "util/size.h"
#include "util/string.h"

#include "fake_spi_flash.h"
#include "fake_system_task.h"

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_print.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

#include "system/logging.h"
#include "system/passert.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define FLASH_SIZE (4 * 1024 * 1024)

void test_flash_logging__initialize(void) {
  fake_spi_flash_init(0, FLASH_SIZE);
}

void test_flash_logging__cleanup(void) {
  uint32_t size = FLASH_REGION_DEBUG_DB_BEGIN - 0;
  fake_flash_assert_region_untouched(0, size);

  size = FLASH_SIZE - FLASH_REGION_DEBUG_DB_END;
  fake_flash_assert_region_untouched(FLASH_REGION_DEBUG_DB_END, size);
  fake_spi_flash_cleanup();
}

typedef struct {
  char **msg_arr;
  int curr_msg_idx;
  int num_items;
  int num_processed;
} ExpectedMessage;

static ExpectedMessage s_msg;

void test_flash_logging_get_info(uint32_t *tot_size, uint32_t *erase_unit_size,
    uint32_t *chunk_size, uint32_t *page_hdr_size);

const uint8_t *version_get_build_id(size_t *out_len) {
  static uint8_t build_id[20] = {
    0xee, 0xd2, 0xbf, 0x50, 0x5b, 0x59, 0x04, 0xb5, 0x14, 0x98,
    0x28, 0xb9, 0x56, 0x6d, 0x26, 0xc5, 0x9b, 0x68, 0xe9, 0xcc };

  if (out_len) {
    *out_len = sizeof(build_id);
  }

  return (uint8_t *)&build_id[0];
}

void version_copy_current_build_id_hex_string(char *buffer,
    size_t buffer_bytes_left) {
  size_t build_id_bytes_left;
  const uint8_t *build_id = version_get_build_id(&build_id_bytes_left);
  byte_stream_to_hex_string(buffer, buffer_bytes_left, build_id,
      build_id_bytes_left, false);
}

int pbl_log_get_bin_format(char* buffer, int buffer_len, const uint8_t log_level,
    const char* src_filename_path, int src_line_number, const char* fmt, ...) {
  va_list fmt_args;
  va_start(fmt_args, fmt);
  int len = vsnprintf(buffer, buffer_len, fmt, fmt_args);
  va_end(fmt_args);
  return (len);
}

static char *get_expected_msg(void) {
  cl_assert(s_msg.msg_arr != NULL);
  cl_assert(s_msg.curr_msg_idx < s_msg.num_items);
  char *expected_msg = NULL;
  if (s_msg.num_processed != 0) {
    expected_msg = s_msg.msg_arr[s_msg.curr_msg_idx - 1];
  }
  s_msg.curr_msg_idx++;
  s_msg.num_processed++;
  return (expected_msg);
}

static bool prv_flash_log_line_dump(uint8_t *msg, uint32_t tot_len) {
  char buf[tot_len + 1];
  memcpy(&buf, (char*)msg, tot_len);
  buf[tot_len] = '\0';
  //  printf("-%s\n", buf);
  char *expected_msg = get_expected_msg();
  if (expected_msg) {
    bool msg_matches_expected = (memcmp(msg, expected_msg, tot_len) == 0);
    cl_assert(msg_matches_expected);
  } else {
    // the first line should always end with the build id
    char build_id_string[64];
    version_copy_current_build_id_hex_string(build_id_string, 64);
    int id_len = strlen(build_id_string);
    char *log_build_id = &buf[tot_len - id_len];

    bool log_contains_build_id =
        (memcmp(log_build_id, build_id_string, id_len) == 0);
    cl_assert(log_contains_build_id);
  }

  return true;
}

bool s_completed = false;
bool s_completed_success = false;
static void prv_flash_log_dump_completed_cb(bool success) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Called prv_flash_log_dump_completed_cb(%d)", (int)success);
  s_completed = true;
  s_completed_success = success;
}

static void setup_and_test_expected_msg(char **msg_arr, int log_gen,
    int start_idx, int num_items) {

  for (int i = start_idx; i < num_items; i++) {
    char *msg = msg_arr[i];
    uint32_t addr = flash_logging_log_start(strlen(msg));
    cl_assert(addr != FLASH_LOG_INVALID_ADDR);

    bool rv = flash_logging_write((uint8_t *)msg, addr, strlen(msg));
    cl_assert(rv);
  }

  ExpectedMessage newmsg = {
    .msg_arr = msg_arr,
    .curr_msg_idx = 0,
    .num_items = (num_items + 1), // + 1 for build id line
    .num_processed = 0,
  };

  s_msg = newmsg;

  s_completed = false;
  bool success = flash_dump_log_file(log_gen, prv_flash_log_line_dump,
                                     prv_flash_log_dump_completed_cb);
  cl_assert(success);
  while (!s_completed) {
    fake_system_task_callbacks_invoke_pending();
  }
  cl_assert(s_completed_success);

  cl_assert_equal_i(s_msg.num_processed, s_msg.num_items);
}

static char **generate_unique_logs(size_t space_avail, size_t log_len,
    int *num_logs) {

  *num_logs = space_avail / (log_len + 2); // add 2 for overhead per log

  char msg[log_len + 1];

  char **msg_arr = malloc(*num_logs * sizeof(char *));

  for (int i = 0; i < *num_logs; i++) {
    uint32_t uniq_msg_id = 0xDEADDEAD - i + (i << 16);

    char buf[20];
    snprintf(buf, sizeof(buf), "%x", uniq_msg_id);

    for (int j = 0; j < log_len; j += strlen(buf)) {
      memcpy(&msg[j], buf, MIN(strlen(buf), log_len - j));
    }

    msg[log_len] = '\0';
    msg_arr[i] = task_strdup(msg);
  }

  return (msg_arr);
}

static void free_logs(char **msg_arr, int num_logs) {
  for (int i = 0; i < num_logs; i++) {
    free(msg_arr[i]);
  }
  free(msg_arr);
}


//
// Actual Tests
//

//! Simple test to confirm that we can log and read back several messages
void test_flash_logging__basic(void) {
  flash_logging_init();

  const char *test_messages[] = {
    "A simple test log message! Woohoo!",
    "Another message",
    "ABCDEFG 0123456789",
    "Last simple test message"
  };

  int num_messages = ARRAY_LENGTH(test_messages);

  setup_and_test_expected_msg((char **)test_messages, 0, 0, num_messages);
}

//! Auto generate unique log messages of uniform length which span multiple log
//! chunks. Try several log message lengths
void test_flash_logging__multi_region(void) {
  const uint32_t header_overhead = 8 * 28;
  const uint32_t space_avail = 64 * 1024 - header_overhead;

  for (int log_len = 2; log_len < 128; log_len += 3) {
    test_flash_logging__cleanup();
    test_flash_logging__initialize();
    flash_logging_init();
    int num_logs;
    char **logs = generate_unique_logs(space_avail, log_len, &num_logs);
    setup_and_test_expected_msg(logs, 0, 0, num_logs);
    free_logs(logs, num_logs);
  }
}

//!
void test_flash_logging__wrap(void) {
  flash_logging_init();

  uint32_t tot_size, erase_size, page_size, page_hdr_size;

  test_flash_logging_get_info(&tot_size, &erase_size, &page_size,
      &page_hdr_size);

  int num_pages = tot_size / page_size;

  uint32_t space_avail = tot_size - num_pages * page_hdr_size;

  // make sure the logs are of an appropriate size such that each page will
  // be entirely filled
  int log_len = 2;
  cl_assert((page_size - page_hdr_size) % (2 + log_len) == 0);

  // fill up all of our log record space
  int num_logs;
  char **logs = generate_unique_logs(space_avail, log_len, &num_logs);
  setup_and_test_expected_msg(logs, 0, 0, num_logs);

  // write two more additional logs which should cause the first erase
  // region to get erased
  logs = realloc(logs, sizeof(char *) * (num_logs + 2));
  logs[num_logs] = task_strdup("Let's test if wrap around is working!");
  logs[num_logs + 1] = task_strdup("This should be on an early page");

  int start_log = num_logs / (tot_size / erase_size);
  int num_wrapped = num_logs - start_log;
  setup_and_test_expected_msg(&logs[start_log], 0, num_wrapped, num_wrapped + 2);

  free_logs(logs, num_logs + 2);
}

//! Keep simulating reboots and generating new logs. Confirm that
//! the most recent generations are not removed during reboots.
void test_flash_logging__generations(void) {

  uint32_t tot_size, erase_size, page_size, page_hdr_size;
  test_flash_logging_get_info(&tot_size, &erase_size, &page_size,
      &page_hdr_size);

  int gens_avail = (tot_size - erase_size) / page_size;

  char *log = malloc(100);
  for (int i = 0; i < 533; i++) {
    flash_logging_init();

    // Write the new message
    snprintf(log, 100, "Generation 0x%x", i);
    setup_and_test_expected_msg(&log, 0, 0, 1);

    for (int gen = 0; gen < MIN(gens_avail, i); gen++) {
      // Check to make sure the most recent log gens are around
      snprintf(log, 100, "Generation 0x%x", i - gen);
      setup_and_test_expected_msg(&log, gen, 1, 1);
    }
  }

  free(log);
}

//! Test the case where the most recent log generation has wrapped the logging
//! region many times.  Confirm that upon reboot, the most recent messages from
//! that generation remain
static int s_long_lived_last_val = -1;
static bool flash_log_line_dump_long_lived(uint8_t *msg, uint32_t tot_len) {
  static bool got_first_line = false;
  if (!got_first_line) {
    got_first_line = true;
    return (true);
  }

  char buf[tot_len + 1];
  memcpy(&buf, (char*)msg, tot_len);
  buf[tot_len] = '\0';

  PBL_LOG(LOG_LEVEL_DEBUG, "flash_log_line_dump_long_lived: got %s", buf);

  int curr_val;
  int filled = sscanf(buf, "Loop Counter %d", &curr_val);
  cl_assert_equal_i(filled, 1);

  if (s_long_lived_last_val != -1) {
    cl_assert_equal_i(s_long_lived_last_val + 1, curr_val);
  }

  s_long_lived_last_val = curr_val;
  PBL_LOG(LOG_LEVEL_DEBUG, "flash_log_line_dump_long_lived: got %s, last_val:%d", buf,
          s_long_lived_last_val);

  return (true);
}

void test_flash_logging__long_lived_log(void) {
  flash_logging_init();

  uint32_t start_addr = flash_logging_log_start(1);
  char *msg = "h";
  bool rv = flash_logging_write((uint8_t *)msg, start_addr, 1);
  cl_assert(rv);

  uint32_t tot_size, erase_size, page_size, page_hdr_size;
  test_flash_logging_get_info(&tot_size, &erase_size, &page_size,
      &page_hdr_size);

  uint32_t addr = FLASH_LOG_INVALID_ADDR;
  int loop_count = 0;
  char *log = malloc(100);
  int num_half_wraps = 0;
  int tot_half_wraps = 5; // make odd so we wrap into the middle of the log region

  while (num_half_wraps < tot_half_wraps) {
    loop_count++;
    snprintf(log, 100, "Loop Counter %d", loop_count);
    addr = flash_logging_log_start(strlen(log));
    cl_assert(addr != FLASH_LOG_INVALID_ADDR);
    rv = flash_logging_write((uint8_t *)log, addr, strlen(log));
    cl_assert(rv);

    if (addr == start_addr || (addr == (start_addr + erase_size - page_size))) {
      num_half_wraps++;
    }

  }

  // simulate a reboot
  flash_logging_init();

  // check to see that the most recent messages (largest loop count numbers)
  // are left
  s_completed = false;
  rv = flash_dump_log_file(1, flash_log_line_dump_long_lived, prv_flash_log_dump_completed_cb);
  cl_assert(rv);

  while (!s_completed) {
    fake_system_task_callbacks_invoke_pending();
  }
  cl_assert(s_completed_success);


  cl_assert_equal_i(s_long_lived_last_val, loop_count);
}

//! Check error handling for some of the different edge cases
void test_flash_logging__errors(void) {
  flash_logging_init();

  uint32_t start_addr = flash_logging_log_start(0);
  cl_assert_equal_i(start_addr, FLASH_LOG_INVALID_ADDR);

  char *msg = "0123456789";
  start_addr = flash_logging_log_start(10);
  cl_assert(start_addr != FLASH_LOG_INVALID_ADDR);
  bool rv = flash_logging_write((uint8_t *)msg, start_addr, strlen(msg));
  cl_assert(rv);

  rv = flash_logging_write((uint8_t *)msg, start_addr, strlen(msg));
  cl_assert(!rv);

  setup_and_test_expected_msg(&msg, 0, 1, 1);
}

//! Make sure that when we chunk up our writes for a log message, the lines
//! are saved as expected and that bogus writes after a record has been written
//! do not take
void test_flash_logging__multi_writes_per_log(void) {
  flash_logging_init();

  const int log_len = 49;
  int num_logs = 0;
  char **logs = generate_unique_logs(20222, log_len, &num_logs);

  for (int i = 0; i < num_logs; i++) {
    uint8_t write_sizes[4] = { log_len / 2, log_len / 4, log_len / 8, log_len };

    int bytes_remaining = log_len;
    uint32_t addr = flash_logging_log_start(log_len);
    cl_assert(addr != FLASH_LOG_INVALID_ADDR);
    char *curr_msg = logs[i];
    bool rv;
    for (int j = 0; j < ARRAY_LENGTH(write_sizes); j++) {
      int bytes_to_write = MIN(bytes_remaining, write_sizes[j]);
      int offset = log_len - bytes_remaining;

      rv = flash_logging_write((uint8_t *)&curr_msg[offset], addr,
          bytes_to_write);
      cl_assert(rv);
      bytes_remaining -= bytes_to_write;
    }

    // try to write something past the end to ensure it doesn't take
    uint8_t buf[128] = { 0 };
    rv = flash_logging_write(&buf[0], addr, sizeof(buf));
    cl_assert(!rv);
  }

  setup_and_test_expected_msg(logs, 0, num_logs, num_logs);
}
