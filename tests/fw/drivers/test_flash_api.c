/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_new_timer.h>

#define BASE           0x10000000U
#define SECTOR_SIZE    0x10000U
#define SUBSECTOR_SIZE 0x1000U

struct erase_cmd {
  uint32_t addr;
  uint32_t size;
};

static struct erase_cmd s_erases[64];
static int s_num_erases;
static bool s_blank;
static int s_erase_status;
static int s_erase_errors;
static int s_suspends;
static int s_resumes;
static int s_writes;
static int s_power_downs;
static int s_erase_ret;
static int s_status_calls;

static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  memset(buf, s_blank ? 0xFF : 0x00, len);
  return 0;
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  s_writes++;
  return 0;
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  cl_assert(s_num_erases < (int)ARRAY_LENGTH(s_erases));
  s_erases[s_num_erases++] = (struct erase_cmd){.addr = addr, .size = size};
  return s_erase_ret;
}

static int prv_erase_status(const struct pbl_flash *flash) {
  s_status_calls++;
  if (s_erase_errors > 0) {
    s_erase_errors--;
    return -EIO;
  }
  return s_erase_status;
}

static int prv_erase_suspend(const struct pbl_flash *flash) {
  s_suspends++;
  return 0;
}

static int prv_erase_resume(const struct pbl_flash *flash) {
  s_resumes++;
  return 0;
}

static const struct pbl_flash_ops s_sync_ops = {
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
};

static int s_fn_calls;
static int s_stops;

static void prv_power_down(const struct pbl_flash *flash, pbl_flash_pm_fn_t fn, void *arg) {
  s_power_downs++;
  fn(arg);
}

static void prv_stop(const struct pbl_flash *flash) {
  s_stops++;
}

static void prv_fn(void *arg) {
  s_fn_calls++;
}

static const struct pbl_flash_ops s_async_ops = {
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
  .erase_status = prv_erase_status,
  .erase_suspend = prv_erase_suspend,
  .erase_resume = prv_erase_resume,
  .power_down = prv_power_down,
  .stop = prv_stop,
};

static const struct pbl_flash_geometry s_geometry = {
  .size = 0x100000U,
  .page_size = 256U,
  .sector_size = SECTOR_SIZE,
  .subsector_size = SUBSECTOR_SIZE,
  .sector_erase_ms = 100U,
  .subsector_erase_ms = 100U,
};

PBL_FLASH_STATE_DEFINE(s_sync);
static const struct pbl_flash s_sync = PBL_FLASH_INIT(s_sync, "sync", nullptr, nullptr, &s_sync_ops,
                                                      BASE, &s_geometry, nullptr, nullptr);
PBL_DEVICE_REGISTER(s_sync, &s_sync.dev);

PBL_FLASH_STATE_DEFINE(s_async);
static const struct pbl_flash s_async = PBL_FLASH_INIT(
    s_async, "async", nullptr, nullptr, &s_async_ops, BASE, &s_geometry, nullptr, nullptr);
PBL_DEVICE_REGISTER(s_async, &s_async.dev);

static int s_cb_status;
static int s_cb_calls;
static void *s_cb_arg;

static void prv_cb(int status, void *arg) {
  s_cb_status = status;
  s_cb_arg = arg;
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

void test_flash_api__initialize(void) {
  s_sync_device_state = (struct pbl_device_state){};
  s_async_device_state = (struct pbl_device_state){};
  s_sync_flash_state = (struct pbl_flash_state){};
  s_async_flash_state = (struct pbl_flash_state){};

  s_num_erases = 0;
  s_blank = false;
  s_erase_status = 0;
  s_erase_errors = 0;
  s_suspends = 0;
  s_resumes = 0;
  s_writes = 0;
  s_power_downs = 0;
  s_fn_calls = 0;
  s_stops = 0;
  s_erase_ret = 0;
  s_status_calls = 0;
  s_cb_status = 1;
  s_cb_calls = 0;
  s_cb_arg = nullptr;

  cl_assert_equal_i(pbl_device_init(&s_sync.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_async.dev), 0);
}

void test_flash_api__cleanup(void) {
  stub_new_timer_cleanup();
}

void test_flash_api__erase_subsector(void) {
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE + 0x3000U, SUBSECTOR_SIZE), 0);
  cl_assert_equal_i(s_num_erases, 1);
  cl_assert_equal_i(s_erases[0].addr, BASE + 0x3000U);
  cl_assert_equal_i(s_erases[0].size, SUBSECTOR_SIZE);
}

void test_flash_api__erase_sector(void) {
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE + SECTOR_SIZE, SECTOR_SIZE), 0);
  cl_assert_equal_i(s_num_erases, 1);
  cl_assert_equal_i(s_erases[0].addr, BASE + SECTOR_SIZE);
  cl_assert_equal_i(s_erases[0].size, SECTOR_SIZE);
}

void test_flash_api__erase_rounds_length_up(void) {
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE, 1U), 0);
  cl_assert_equal_i(s_num_erases, 1);
  cl_assert_equal_i(s_erases[0].size, SUBSECTOR_SIZE);
}

void test_flash_api__erase_uses_sectors_where_possible(void) {
  // 2 leading subsectors, 2 sectors, 1 trailing subsector
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE + 0xE000U, 0x31000U - 0xE000U), 0);
  cl_assert_equal_i(s_num_erases, 5);
  cl_assert_equal_i(s_erases[0].addr, BASE + 0xE000U);
  cl_assert_equal_i(s_erases[0].size, SUBSECTOR_SIZE);
  cl_assert_equal_i(s_erases[1].addr, BASE + 0xF000U);
  cl_assert_equal_i(s_erases[1].size, SUBSECTOR_SIZE);
  cl_assert_equal_i(s_erases[2].addr, BASE + 0x10000U);
  cl_assert_equal_i(s_erases[2].size, SECTOR_SIZE);
  cl_assert_equal_i(s_erases[3].addr, BASE + 0x20000U);
  cl_assert_equal_i(s_erases[3].size, SECTOR_SIZE);
  cl_assert_equal_i(s_erases[4].addr, BASE + 0x30000U);
  cl_assert_equal_i(s_erases[4].size, SUBSECTOR_SIZE);
}

void test_flash_api__erase_only_subsectors_when_no_sector_fits(void) {
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE + 0x1000U, 0x3000U), 0);
  cl_assert_equal_i(s_num_erases, 3);
  for (int i = 0; i < 3; i++) {
    cl_assert_equal_i(s_erases[i].addr, BASE + 0x1000U + i * SUBSECTOR_SIZE);
    cl_assert_equal_i(s_erases[i].size, SUBSECTOR_SIZE);
  }
}

void test_flash_api__erase_skips_blank_units(void) {
  s_blank = true;
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE, 2U * SECTOR_SIZE), 0);
  cl_assert_equal_i(s_num_erases, 0);
}

void test_flash_api__erase_rejects_bad_ranges(void) {
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE + 0x800U, SUBSECTOR_SIZE), -EINVAL);
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE - SUBSECTOR_SIZE, SUBSECTOR_SIZE), -EINVAL);
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE + s_geometry.size, SUBSECTOR_SIZE), -EINVAL);
  cl_assert_equal_i(s_num_erases, 0);
}

void test_flash_api__access_out_of_range(void) {
  uint8_t buf[4];

  cl_assert_equal_i(pbl_flash_read(&s_sync, BASE + s_geometry.size - 2U, buf, sizeof(buf)),
                    -EINVAL);
  cl_assert_equal_i(pbl_flash_write(&s_sync, BASE - 1U, buf, sizeof(buf)), -EINVAL);
  cl_assert_equal_i(s_writes, 0);
}

void test_flash_api__erase_async_completes_through_callback(void) {
  cl_assert_equal_i(pbl_flash_erase_async(&s_sync, BASE, 2U * SECTOR_SIZE, prv_cb, (void *)0x1234),
                    0);
  prv_fire_timers(10);
  cl_assert_equal_i(s_num_erases, 2);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, 0);
  cl_assert_equal_p(s_cb_arg, (void *)0x1234);
}

void test_flash_api__erase_async_empty_range(void) {
  cl_assert_equal_i(pbl_flash_erase_async(&s_sync, BASE, 0U, prv_cb, nullptr), 0);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, 0);
  cl_assert_equal_i(s_num_erases, 0);
}

void test_flash_api__async_driver_polls_status(void) {
  s_erase_status = -EBUSY;

  cl_assert_equal_i(
      pbl_flash_erase_async(&s_async, BASE + SECTOR_SIZE, SECTOR_SIZE, prv_cb, nullptr), 0);
  cl_assert_equal_i(s_num_erases, 1);
  cl_assert_equal_i(s_cb_calls, 0);

  prv_fire_timers(3);
  cl_assert_equal_i(s_cb_calls, 0);

  s_erase_status = 0;
  prv_fire_timers(1);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, 0);
}

void test_flash_api__erase_retried_on_error(void) {
  s_erase_errors = 1;

  cl_assert_equal_i(pbl_flash_erase(&s_async, BASE + SECTOR_SIZE, SECTOR_SIZE), 0);
  cl_assert_equal_i(s_num_erases, 2);
  cl_assert_equal_i(s_erases[1].addr, BASE + SECTOR_SIZE);
}

void test_flash_api__erase_gives_up_after_retries(void) {
  s_erase_errors = 100;

  cl_assert_equal_i(
      pbl_flash_erase_async(&s_async, BASE + SECTOR_SIZE, SECTOR_SIZE, prv_cb, nullptr), 0);
  prv_fire_timers(20);
  cl_assert_equal_i(s_cb_calls, 1);
  cl_assert_equal_i(s_cb_status, -EIO);
  // The first attempt and three retries
  cl_assert_equal_i(s_num_erases, 4);
}

void test_flash_api__read_suspends_and_resumes_erase(void) {
  uint8_t buf[4];

  s_erase_status = -EBUSY;

  cl_assert_equal_i(
      pbl_flash_erase_async(&s_async, BASE + SECTOR_SIZE, SECTOR_SIZE, prv_cb, nullptr), 0);

  cl_assert_equal_i(pbl_flash_read(&s_async, BASE, buf, sizeof(buf)), 0);
  cl_assert_equal_i(s_suspends, 1);
  cl_assert_equal_i(s_resumes, 0);
  cl_assert(stub_new_timer_is_scheduled(s_async_flash_state.resume_timer));

  // Further reads while suspended do not suspend again
  cl_assert_equal_i(pbl_flash_read(&s_async, BASE, buf, sizeof(buf)), 0);
  cl_assert_equal_i(s_suspends, 1);

  stub_new_timer_fire(s_async_flash_state.resume_timer);
  cl_assert_equal_i(s_resumes, 1);
}

void test_flash_api__is_erased(void) {
  cl_assert(!pbl_flash_is_erased(&s_sync, BASE, SUBSECTOR_SIZE));
  s_blank = true;
  cl_assert(pbl_flash_is_erased(&s_sync, BASE, SUBSECTOR_SIZE));
}

void test_flash_api__protected_range(void) {
  uint8_t buf[4] = {};

  cl_assert_equal_i(pbl_flash_protect(&s_sync, BASE + SECTOR_SIZE, SECTOR_SIZE), 0);

  cl_assert_equal_i(
      pbl_flash_erase_async(&s_sync, BASE + SECTOR_SIZE, SECTOR_SIZE, prv_cb, nullptr), -EACCES);
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE, 2U * SECTOR_SIZE), -EACCES);
  cl_assert_equal_i(pbl_flash_write(&s_sync, BASE + SECTOR_SIZE - 2U, buf, sizeof(buf)), -EACCES);
  cl_assert_equal_i(s_num_erases, 0);
  cl_assert_equal_i(s_writes, 0);

  cl_assert_equal_i(pbl_flash_unprotect(&s_sync), 0);
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE + SECTOR_SIZE, SECTOR_SIZE), 0);
  cl_assert_equal_i(pbl_flash_write(&s_sync, BASE + SECTOR_SIZE, buf, sizeof(buf)), 0);
  cl_assert_equal_i(s_num_erases, 1);
  cl_assert_equal_i(s_writes, 1);
}

void test_flash_api__not_ready(void) {
  uint8_t buf[4];

  s_sync_device_state = (struct pbl_device_state){};

  cl_assert_equal_i(pbl_flash_read(&s_sync, BASE, buf, sizeof(buf)), -ENODEV);
  cl_assert_equal_i(pbl_flash_erase(&s_sync, BASE, SUBSECTOR_SIZE), -ENODEV);
}

void test_flash_api__coredump_mode_is_synchronous(void) {
  cl_assert_equal_i(pbl_flash_coredump_init(&s_async), 0);

  cl_assert_equal_i(pbl_flash_erase(&s_async, BASE, SECTOR_SIZE + SUBSECTOR_SIZE), 0);
  cl_assert_equal_i(s_num_erases, 2);
  cl_assert_equal_i(s_erases[0].size, SECTOR_SIZE);
  cl_assert_equal_i(s_erases[1].size, SUBSECTOR_SIZE);
  cl_assert_equal_i(s_suspends, 0);
  cl_assert_equal_i(pbl_flash_erase_async(&s_async, BASE, SUBSECTOR_SIZE, prv_cb, nullptr), -EPERM);
}

void test_flash_api__stays_powered_during_erase(void) {
  s_erase_status = -EBUSY;

  cl_assert_equal_i(
      pbl_flash_erase_async(&s_async, BASE + SECTOR_SIZE, SECTOR_SIZE, prv_cb, nullptr), 0);
  pbl_flash_power_down(&s_async, prv_fn, nullptr);
  cl_assert_equal_i(s_power_downs, 0);
  cl_assert_equal_i(s_fn_calls, 1);

  s_erase_status = 0;
  prv_fire_timers(1);
  cl_assert_equal_i(s_cb_calls, 1);

  pbl_flash_power_down(&s_async, prv_fn, nullptr);
  cl_assert_equal_i(s_power_downs, 1);
  cl_assert_equal_i(s_fn_calls, 2);
}

void test_flash_api__stop_refuses_access(void) {
  uint8_t byte;

  pbl_flash_stop(&s_async);
  cl_assert_equal_i(s_stops, 1);

  cl_assert_equal_i(pbl_flash_read(&s_async, BASE, &byte, 1U), -ENODEV);
  cl_assert_equal_i(pbl_flash_write(&s_async, BASE, &byte, 1U), -ENODEV);
  cl_assert_equal_i(pbl_flash_erase(&s_async, BASE, SUBSECTOR_SIZE), -ENODEV);

  pbl_flash_stop(&s_async);
  cl_assert_equal_i(s_stops, 1);

  cl_assert_equal_i(pbl_flash_coredump_init(&s_async), 0);
  cl_assert_equal_i(pbl_flash_read(&s_async, BASE, &byte, 1U), 0);
}

void test_flash_api__erase_completed_by_driver(void) {
  s_erase_ret = 1;

  cl_assert_equal_i(pbl_flash_erase(&s_async, BASE, SECTOR_SIZE + SUBSECTOR_SIZE), 0);
  cl_assert_equal_i(s_num_erases, 2);
  cl_assert_equal_i(s_status_calls, 0);
}
