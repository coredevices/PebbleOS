/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/reset.h>

#include <clar.h>

static int s_lock_depth;
static char s_log[16];
static int s_assert_ret;

void pbl_irq_lock(void) {
  s_lock_depth++;
}

void pbl_irq_unlock(void) {
  s_lock_depth--;
}

static void prv_log(char c) {
  size_t n = strlen(s_log);

  s_log[n] = c;
  s_log[n + 1] = '\0';
}

static int prv_assert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  cl_assert(s_lock_depth > 0);
  cl_assert_equal_i(id, 2);
  prv_log('a');
  return s_assert_ret;
}

static int prv_deassert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  cl_assert(s_lock_depth > 0);
  cl_assert_equal_i(id, 2);
  prv_log('d');
  return 0;
}

static const struct pbl_reset_controller_ops s_ops = {
  .assert = prv_assert,
  .deassert = prv_deassert,
};

PBL_RESET_CONTROLLER_STATE_DEFINE(s_ctrl);
static const struct pbl_reset_controller s_ctrl =
    PBL_RESET_CONTROLLER_INIT(s_ctrl, "rst", nullptr, nullptr, &s_ops, 4);

static const struct pbl_reset s_rst = PBL_RESET(&s_ctrl, 2);
static const struct pbl_reset s_rst_bad = PBL_RESET(&s_ctrl, 4);

void test_reset__initialize(void) {
  s_lock_depth = 0;
  s_log[0] = '\0';
  s_assert_ret = 0;
  cl_assert_equal_i(pbl_device_init(&s_ctrl.dev), 0);
}

void test_reset__assert_deassert(void) {
  cl_assert_equal_i(pbl_reset_assert(&s_rst), 0);
  cl_assert_equal_i(pbl_reset_deassert(&s_rst), 0);
  cl_assert_equal_s(s_log, "ad");
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_reset__pulse(void) {
  cl_assert_equal_i(pbl_reset_reset(&s_rst), 0);
  cl_assert_equal_s(s_log, "ad");
}

void test_reset__pulse_stops_on_failure(void) {
  s_assert_ret = -EIO;
  cl_assert_equal_i(pbl_reset_reset(&s_rst), -EIO);
  cl_assert_equal_s(s_log, "a");
}

void test_reset__invalid_id(void) {
  cl_assert_equal_i(pbl_reset_assert(&s_rst_bad), -EINVAL);
  cl_assert_equal_i(pbl_reset_deassert(&s_rst_bad), -EINVAL);
  cl_assert_equal_s(s_log, "");
}
