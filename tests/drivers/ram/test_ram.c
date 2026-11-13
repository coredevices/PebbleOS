/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/ram.h>

#include <clar.h>
#include <fake_mutex.h>

static unsigned int s_on;
static unsigned int s_off;
static unsigned int s_retain;
static unsigned int s_resume;
static unsigned int s_quiesce;
static int s_on_ret;

static int prv_init(const struct pbl_ram *ram) {
  ram->state->base = 0x60000000U;
  ram->state->size = 0x100000U;
  return 0;
}

static int prv_power_on(const struct pbl_ram *ram) {
  if (s_on_ret == 0) {
    s_on++;
  }
  return s_on_ret;
}

static void prv_power_off(const struct pbl_ram *ram) {
  s_off++;
}

static void prv_retain(const struct pbl_ram *ram) {
  s_retain++;
}

static void prv_resume(const struct pbl_ram *ram) {
  s_resume++;
}

static void prv_quiesce(const struct pbl_ram *ram) {
  s_quiesce++;
}

static const struct pbl_ram_ops s_ops = {
  .init = prv_init,
  .power_on = prv_power_on,
  .power_off = prv_power_off,
  .retain = prv_retain,
  .resume = prv_resume,
  .quiesce = prv_quiesce,
};

PBL_RAM_STATE_DEFINE(s_ram);
static const struct pbl_ram s_ram = PBL_RAM_INIT(s_ram, "ram", nullptr, nullptr, &s_ops);

static void prv_deep_sleep(void) {
  pbl_ram_suspend(&s_ram);
  pbl_ram_resume(&s_ram);
}

void test_ram__initialize(void) {
  s_on = s_off = s_retain = s_resume = s_quiesce = 0U;
  s_on_ret = 0;
  s_ram_ram_state.users = 0U;
  s_ram_ram_state.on = false;
  s_ram_ram_state.retained = false;
  cl_assert_equal_i(pbl_device_init(&s_ram.dev), 0);
}

void test_ram__cleanup(void) {
  fake_mutex_assert_all_unlocked();
}

void test_ram__region_from_the_driver(void) {
  uintptr_t base;
  size_t size;

  pbl_ram_get_region(&s_ram, &base, &size);
  cl_assert_equal_i(base, 0x60000000U);
  cl_assert_equal_i(size, 0x100000U);
}

void test_ram__first_user_powers_on(void) {
  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  cl_assert_equal_i(s_on, 1);

  pbl_ram_put(&s_ram);
  pbl_ram_put(&s_ram);
  // Still on until deep sleep
  cl_assert_equal_i(s_off, 0);
}

void test_ram__off_in_deep_sleep_without_users(void) {
  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  pbl_ram_put(&s_ram);

  prv_deep_sleep();
  cl_assert_equal_i(s_off, 1);
  cl_assert_equal_i(s_retain, 0);
  cl_assert_equal_i(s_resume, 0);

  // Already off
  prv_deep_sleep();
  cl_assert_equal_i(s_off, 1);

  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  cl_assert_equal_i(s_on, 2);
  pbl_ram_put(&s_ram);
}

void test_ram__reused_before_deep_sleep_stays_on(void) {
  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  pbl_ram_put(&s_ram);
  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  cl_assert_equal_i(s_on, 1);
  cl_assert_equal_i(s_off, 0);
  pbl_ram_put(&s_ram);
}

void test_ram__retained_in_deep_sleep_with_users(void) {
  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);

  prv_deep_sleep();
  cl_assert_equal_i(s_retain, 1);
  cl_assert_equal_i(s_resume, 1);
  cl_assert_equal_i(s_off, 0);

  pbl_ram_put(&s_ram);
}

void test_ram__power_on_failure(void) {
  s_on_ret = -EIO;
  cl_assert_equal_i(pbl_ram_get(&s_ram), -EIO);
  cl_assert_equal_i(s_ram_ram_state.users, 0);

  s_on_ret = 0;
  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  pbl_ram_put(&s_ram);
}

void test_ram__quiesced_only_while_on(void) {
  pbl_ram_quiesce(&s_ram);
  cl_assert_equal_i(s_quiesce, 0);

  cl_assert_equal_i(pbl_ram_get(&s_ram), 0);
  pbl_ram_quiesce(&s_ram);
  cl_assert_equal_i(s_quiesce, 1);
  pbl_ram_put(&s_ram);
}
