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

static uint64_t s_rates_mhz[2] = {32768000U, 9658250U};

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t *rate_mhz) {
  cl_assert(s_lock_depth > 0);
  *rate_mhz = s_rates_mhz[id];
  return 0;
}

static const struct pbl_clk_controller_ops s_osc_ops = {
  .get_rate = prv_get_rate,
};

PBL_CLK_CONTROLLER_STATE_DEFINE(s_osc, 2);
static const struct pbl_clk_controller s_osc =
    PBL_CLK_CONTROLLER_INIT(s_osc, "osc", nullptr, nullptr, &s_osc_ops);

static const struct pbl_clk s_osc0 = PBL_CLK(&s_osc, 0);
static const struct pbl_clk s_osc1 = PBL_CLK(&s_osc, 1);

void test_clk__rate(void) {
  uint64_t rate_mhz = 0U;

  cl_assert_equal_i(pbl_device_init(&s_osc.dev), 0);

  cl_assert_equal_i(pbl_clk_get_rate(&s_osc1, &rate_mhz), 0);
  cl_assert_equal_i(rate_mhz, 9658250U);
  cl_assert_equal_i(pbl_clk_get_rate(&s_clk0, &rate_mhz), -ENOTSUP);
  cl_assert_equal_i(pbl_clk_get_rate(&s_clk_bad, &rate_mhz), -EINVAL);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__always_on(void) {
  cl_assert_equal_i(pbl_device_init(&s_osc.dev), 0);

  cl_assert_equal_i(pbl_clk_enable(&s_osc0), 0);
  cl_assert(pbl_clk_is_enabled(&s_osc0));
  cl_assert_equal_i(pbl_clk_disable(&s_osc0), 0);
  cl_assert(!pbl_clk_is_enabled(&s_osc0));
}

static int s_calls[2];
static uint64_t s_last_mhz;

static void prv_rate_changed(struct pbl_clk_notifier *nb, uint64_t rate_mhz) {
  s_calls[nb->clk.id]++;
  s_last_mhz = rate_mhz;
}

static struct pbl_clk_notifier s_nb0 = {.clk = PBL_CLK(&s_osc, 0), .fn = prv_rate_changed};
static struct pbl_clk_notifier s_nb1 = {.clk = PBL_CLK(&s_osc, 1), .fn = prv_rate_changed};

void test_clk__notifier(void) {
  pbl_clk_notifier_register(&s_nb0);
  pbl_clk_notifier_register(&s_nb1);
  cl_assert_equal_i(s_lock_depth, 0);

  pbl_clk_rate_changed(&s_osc, 1, 9658300U);
  cl_assert_equal_i(s_calls[0], 0);
  cl_assert_equal_i(s_calls[1], 1);
  cl_assert_equal_i(s_last_mhz, 9658300U);

  pbl_clk_rate_changed(&s_ctrl, 1, 1000U);
  cl_assert_equal_i(s_calls[1], 1);
  cl_assert_equal_i(s_lock_depth, 0);
}
