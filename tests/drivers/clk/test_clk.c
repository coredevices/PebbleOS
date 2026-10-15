/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/clk.h>

#include <clar.h>

static int s_lock_depth;
static uint32_t s_gates;
static int s_enable_calls;
static int s_disable_calls;
static int s_enable_ret;
static int s_disable_ret;

void pbl_irq_lock(void) {
  s_lock_depth++;
}

void pbl_irq_unlock(void) {
  s_lock_depth--;
}

static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  cl_assert(s_lock_depth > 0);
  s_enable_calls++;
  if (s_enable_ret == 0) {
    s_gates |= 1U << id;
  }
  return s_enable_ret;
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  cl_assert(s_lock_depth > 0);
  s_disable_calls++;
  if (s_disable_ret == 0) {
    s_gates &= ~(1U << id);
  }
  return s_disable_ret;
}

static const struct pbl_clk_controller_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
};

PBL_CLK_CONTROLLER_STATE_DEFINE(s_ctrl, 4);
static const struct pbl_clk_controller s_ctrl =
    PBL_CLK_CONTROLLER_INIT(s_ctrl, "clk", nullptr, nullptr, &s_ops);

static const struct pbl_clk s_clk0 = PBL_CLK(&s_ctrl, 0);
static const struct pbl_clk s_clk3 = PBL_CLK(&s_ctrl, 3);
static const struct pbl_clk s_clk_bad = PBL_CLK(&s_ctrl, 4);

void test_clk__initialize(void) {
  s_lock_depth = 0;
  s_gates = 0U;
  s_enable_calls = 0;
  s_disable_calls = 0;
  s_enable_ret = 0;
  s_disable_ret = 0;
  memset(s_ctrl_clk_use_counts, 0, sizeof(s_ctrl_clk_use_counts));
  cl_assert_equal_i(pbl_device_init(&s_ctrl.dev), 0);
}

void test_clk__num_clks(void) {
  cl_assert_equal_i(s_ctrl.num_clks, 4);
}

void test_clk__refcount(void) {
  cl_assert(!pbl_clk_is_enabled(&s_clk0));

  cl_assert_equal_i(pbl_clk_enable(&s_clk0), 0);
  cl_assert_equal_i(pbl_clk_enable(&s_clk0), 0);
  cl_assert_equal_i(s_enable_calls, 1);
  cl_assert_equal_i(s_gates, 0x1);
  cl_assert(pbl_clk_is_enabled(&s_clk0));

  cl_assert_equal_i(pbl_clk_disable(&s_clk0), 0);
  cl_assert_equal_i(s_disable_calls, 0);
  cl_assert_equal_i(s_gates, 0x1);

  cl_assert_equal_i(pbl_clk_disable(&s_clk0), 0);
  cl_assert_equal_i(s_disable_calls, 1);
  cl_assert_equal_i(s_gates, 0x0);
  cl_assert(!pbl_clk_is_enabled(&s_clk0));

  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__independent(void) {
  cl_assert_equal_i(pbl_clk_enable(&s_clk0), 0);
  cl_assert_equal_i(pbl_clk_enable(&s_clk3), 0);
  cl_assert_equal_i(s_gates, 0x9);

  cl_assert_equal_i(pbl_clk_disable(&s_clk3), 0);
  cl_assert_equal_i(s_gates, 0x1);
  cl_assert(pbl_clk_is_enabled(&s_clk0));
}

void test_clk__enable_failure(void) {
  s_enable_ret = -EIO;
  cl_assert_equal_i(pbl_clk_enable(&s_clk0), -EIO);
  cl_assert(!pbl_clk_is_enabled(&s_clk0));

  s_enable_ret = 0;
  cl_assert_equal_i(pbl_clk_enable(&s_clk0), 0);
  cl_assert_equal_i(s_enable_calls, 2);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__disable_failure(void) {
  cl_assert_equal_i(pbl_clk_enable(&s_clk0), 0);

  s_disable_ret = -EIO;
  cl_assert_equal_i(pbl_clk_disable(&s_clk0), -EIO);
  cl_assert(pbl_clk_is_enabled(&s_clk0));
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__invalid_id(void) {
  cl_assert_equal_i(pbl_clk_enable(&s_clk_bad), -EINVAL);
  cl_assert_equal_i(pbl_clk_disable(&s_clk_bad), -EINVAL);
  cl_assert(!pbl_clk_is_enabled(&s_clk_bad));
  cl_assert_equal_i(s_enable_calls, 0);
}
