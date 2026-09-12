/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "fake_new_timer.h"
#include "stubs_analytics.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_tasks.h"
#include "stubs_sem.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

#include <pbl/drivers/flash.h>

#include <errno.h>
#include <string.h>

#define SECTOR_SIZE 0x10000
#define SUBSECTOR_SIZE 0x1000

void watchdog_feed(void) {}
void delay_us(uint32_t us) {}

// Fake driver
///////////////////////////////////////////////////////////

typedef struct EraseCommand {
  uint32_t addr;
  size_t size;
} EraseCommand;

static EraseCommand s_erase_commands[64];
static int s_num_erase_commands;
static bool s_blank;
static int s_erase_status_return;
static int s_erase_status_error_count;
static int s_suspend_calls;
static int s_resume_calls;
static int s_read_calls;
static int s_write_calls;

static int prv_init(const struct pbl_flash_device *dev) {
  return 0;
}

static int prv_read(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len) {
  s_read_calls++;
  memset(buf, s_blank ? 0xff : 0x00, len);
  return 0;
}

static int prv_write(const struct pbl_flash_device *dev, uint32_t addr, const void *buf,
                     size_t len) {
  s_write_calls++;
  return 0;
}

static int prv_erase_begin(const struct pbl_flash_device *dev, uint32_t addr, size_t size) {
  cl_assert(s_num_erase_commands < (int)(sizeof(s_erase_commands) / sizeof(s_erase_commands[0])));
  s_erase_commands[s_num_erase_commands++] = (EraseCommand){.addr = addr, .size = size};
  return 0;
}

static int prv_erase_status(const struct pbl_flash_device *dev) {
  if (s_erase_status_error_count > 0) {
    s_erase_status_error_count--;
    return -EIO;
  }
  return s_erase_status_return;
}

static int prv_erase_suspend(const struct pbl_flash_device *dev) {
  s_suspend_calls++;
  return 0;
}

static int prv_erase_resume(const struct pbl_flash_device *dev) {
  s_resume_calls++;
  return 0;
}

static const struct pbl_flash_ops s_sync_ops = {
    .init = prv_init,
    .read = prv_read,
    .write = prv_write,
    .erase_begin = prv_erase_begin,
};

static const struct pbl_flash_ops s_async_ops = {
    .init = prv_init,
    .read = prv_read,
    .write = prv_write,
    .erase_begin = prv_erase_begin,
    .erase_status = prv_erase_status,
    .erase_suspend = prv_erase_suspend,
    .erase_resume = prv_erase_resume,
};

static const struct pbl_flash_geometry s_geometry = {
    .size = 0x100000,
    .page_size = 256,
    .sector_size = SECTOR_SIZE,
    .subsector_size = SUBSECTOR_SIZE,
    .sector_erase_ms = 100,
    .subsector_erase_ms = 100,
};
static struct pbl_flash_device_state s_state;
static struct pbl_flash_device s_dev = {
    .state = &s_state,
    .ops = &s_sync_ops,
    .base = 0,
    .geometry = &s_geometry,
};

static void *s_cb_ctx;
static int s_cb_status;
static int s_cb_calls;

static void prv_cb(void *ctx, int status) {
  s_cb_ctx = ctx;
  s_cb_status = status;
  s_cb_calls++;
}

static void prv_fire_timers(int max) {
  for (int i = 0; i < max; i++) {
    TimerID timer = stub_new_timer_get_next();
    if (timer == TIMER_INVALID_ID) {
      return;
    }
    stub_new_timer_fire(timer);
  }
}

// Tests
///////////////////////////////////////////////////////////

void test_flash_api__initialize(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_dev.ops = &s_sync_ops;
  s_num_erase_commands = 0;
  s_blank = false;
  s_erase_status_return = 0;
  s_erase_status_error_count = 0;
  s_suspend_calls = 0;
  s_resume_calls = 0;
  s_read_calls = 0;
  s_write_calls = 0;
  s_cb_ctx = NULL;
  s_cb_status = -12345;
  s_cb_calls = 0;

  pbl_flash_init(&s_dev);
}

void test_flash_api__cleanup(void) {
  stub_new_timer_cleanup();
}

void test_flash_api__erase_subsector(void) {
  pbl_flash_erase(&s_dev, 0x3000, SUBSECTOR_SIZE);
  cl_assert_equal_i(s_num_erase_commands, 1);
  cl_assert_equal_i(s_erase_commands[0].addr, 0x3000);
  cl_assert_equal_i(s_erase_commands[0].size, SUBSECTOR_SIZE);
}

void test_flash_api__erase_sector(void) {
  pbl_flash_erase(&s_dev, 0x10000, SECTOR_SIZE);
  cl_assert_equal_i(s_num_erase_commands, 1);
  cl_assert_equal_i(s_erase_commands[0].addr, 0x10000);
  cl_assert_equal_i(s_erase_commands[0].size, SECTOR_SIZE);
}

void test_flash_api__erase_rounds_length_up(void) {
  pbl_flash_erase(&s_dev, 0x0, 1);
  cl_assert_equal_i(s_num_erase_commands, 1);
  cl_assert_equal_i(s_erase_commands[0].size, SUBSECTOR_SIZE);
}

void test_flash_api__erase_uses_sectors_where_possible(void) {
  // [0xE000, 0x31000): 2 leading subsectors, 2 sectors, 1 trailing subsector
  pbl_flash_erase(&s_dev, 0xE000, 0x31000 - 0xE000);
  cl_assert_equal_i(s_num_erase_commands, 5);
  cl_assert_equal_i(s_erase_commands[0].addr, 0xE000);
  cl_assert_equal_i(s_erase_commands[0].size, SUBSECTOR_SIZE);
  cl_assert_equal_i(s_erase_commands[1].addr, 0xF000);
  cl_assert_equal_i(s_erase_commands[1].size, SUBSECTOR_SIZE);
  cl_assert_equal_i(s_erase_commands[2].addr, 0x10000);
  cl_assert_equal_i(s_erase_commands[2].size, SECTOR_SIZE);
  cl_assert_equal_i(s_erase_commands[3].addr, 0x20000);
  cl_assert_equal_i(s_erase_commands[3].size, SECTOR_SIZE);
  cl_assert_equal_i(s_erase_commands[4].addr, 0x30000);
  cl_assert_equal_i(s_erase_commands[4].size, SUBSECTOR_SIZE);
}

void test_flash_api__erase_only_subsectors_when_no_full_sector_fits(void) {
  pbl_flash_erase(&s_dev, 0x1000, 0x3000);
  cl_assert_equal_i(s_num_erase_commands, 3);
  for (int i = 0; i < 3; i++) {
    cl_assert_equal_i(s_erase_commands[i].addr, 0x1000 + i * SUBSECTOR_SIZE);
    cl_assert_equal_i(s_erase_commands[i].size, SUBSECTOR_SIZE);
  }
}

void test_flash_api__erase_skips_blank_units(void) {
  s_blank = true;
  pbl_flash_erase(&s_dev, 0x0, 0x20000);
  cl_assert_equal_i(s_num_erase_commands, 0);
}

void test_flash_api__erase_async_completes_through_callback(void) {
  pbl_flash_erase_async(&s_dev, 0x0, 0x20000, prv_cb, (void *)0x1234);
  prv_fire_timers(10);
  cl_assert_equal_i(s_num_erase_commands, 2);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, 0);
  cl_assert_equal_p(s_cb_ctx, (void *)0x1234);
}

void test_flash_api__erase_async_empty_range(void) {
  pbl_flash_erase_async(&s_dev, 0x0, 0, prv_cb, NULL);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, 0);
  cl_assert_equal_i(s_num_erase_commands, 0);
}

void test_flash_api__async_driver_polls_status(void) {
  s_dev.ops = &s_async_ops;
  s_erase_status_return = -EBUSY;

  pbl_flash_erase_async(&s_dev, 0x10000, SECTOR_SIZE, prv_cb, NULL);
  cl_assert_equal_i(s_num_erase_commands, 1);
  cl_assert_equal_i(s_cb_calls, 0);

  prv_fire_timers(3);
  cl_assert_equal_i(s_cb_calls, 0);

  s_erase_status_return = 0;
  prv_fire_timers(1);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, 0);
}

void test_flash_api__retry_erase_on_first_error(void) {
  s_dev.ops = &s_async_ops;
  s_erase_status_error_count = 1;

  pbl_flash_erase(&s_dev, 0x10000, SECTOR_SIZE);
  cl_assert_equal_i(s_num_erase_commands, 2);
  cl_assert_equal_i(s_erase_commands[1].addr, 0x10000);
}

void test_flash_api__handle_uncorrectable_erase_error(void) {
  s_dev.ops = &s_async_ops;
  s_erase_status_error_count = 100;

  pbl_flash_erase_async(&s_dev, 0x10000, SECTOR_SIZE, prv_cb, NULL);
  prv_fire_timers(20);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, -EIO);
  // Initial attempt plus three retries
  cl_assert_equal_i(s_num_erase_commands, 4);
}

void test_flash_api__read_suspends_and_resumes_erase(void) {
  s_dev.ops = &s_async_ops;
  s_erase_status_return = -EBUSY;

  pbl_flash_erase_async(&s_dev, 0x10000, SECTOR_SIZE, prv_cb, NULL);

  uint8_t buf[4];
  pbl_flash_read(&s_dev, 0x0, buf, sizeof(buf));
  cl_assert_equal_i(s_suspend_calls, 1);
  cl_assert_equal_i(s_resume_calls, 0);
  cl_assert(stub_new_timer_is_scheduled(s_state.resume_timer));

  // Further reads while suspended do not suspend again
  pbl_flash_read(&s_dev, 0x0, buf, sizeof(buf));
  cl_assert_equal_i(s_suspend_calls, 1);

  stub_new_timer_fire(s_state.resume_timer);
  cl_assert_equal_i(s_resume_calls, 1);
}

void test_flash_api__write_calls_driver(void) {
  uint8_t buf[4] = {0};
  pbl_flash_write(&s_dev, 0x100, buf, sizeof(buf));
  cl_assert_equal_i(s_write_calls, 1);
}

void test_flash_api__is_erased(void) {
  cl_assert(!pbl_flash_is_erased(&s_dev, 0x0, SUBSECTOR_SIZE));
  s_blank = true;
  cl_assert(pbl_flash_is_erased(&s_dev, 0x0, SUBSECTOR_SIZE));
}

void test_flash_api__erase_fails_in_protected_range(void) {
  s_dev.ops = &s_sync_ops;
  pbl_flash_protect(&s_dev, 0x10000, SECTOR_SIZE);

  pbl_flash_erase_async(&s_dev, 0x10000, SECTOR_SIZE, prv_cb, NULL);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, -EACCES);
  cl_assert_equal_i(s_num_erase_commands, 0);

  pbl_flash_unprotect(&s_dev);
  pbl_flash_erase_async(&s_dev, 0x10000, SECTOR_SIZE, prv_cb, NULL);
  prv_fire_timers(10);
  cl_assert_equal_i(s_cb_calls, 2);
  cl_assert_equal_i(s_cb_status, 0);
  cl_assert_equal_i(s_num_erase_commands, 1);
}

void test_flash_api__coredump_mode_is_synchronous(void) {
  s_dev.ops = &s_async_ops;
  pbl_flash_coredump_init(&s_dev);

  pbl_flash_erase(&s_dev, 0x0, 0x11000);
  cl_assert_equal_i(s_num_erase_commands, 2);
  cl_assert_equal_i(s_erase_commands[0].size, SECTOR_SIZE);
  cl_assert_equal_i(s_erase_commands[1].size, SUBSECTOR_SIZE);
  cl_assert_equal_i(s_suspend_calls, 0);
}
