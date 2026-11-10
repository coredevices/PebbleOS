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

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  cl_assert(s_lock_depth > 0);
  cl_assert_equal_i(parent_mhz, 0U);
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

  s_rates_mhz[1] = 9658300U;
  pbl_clk_rate_changed(&s_osc1);
  cl_assert_equal_i(s_calls[0], 0);
  cl_assert_equal_i(s_calls[1], 1);
  cl_assert_equal_i(s_last_mhz, 9658300U);

  pbl_clk_rate_changed(&(struct pbl_clk)PBL_CLK(&s_ctrl, 1));
  cl_assert_equal_i(s_calls[1], 1);
  cl_assert_equal_i(s_lock_depth, 0);
}

// A tree over two controllers: gates 0 and 1 run from mux 2 of the same controller, which
// selects oscillator 0 or 1 of s_osc, and divides it by 2.
enum {
  TREE_GATE0,
  TREE_GATE1,
  TREE_MUX,
  TREE_NUM,
};

static uint32_t s_mux_sel;
static uint32_t s_mux_div;
static int s_set_parent_ret;
static uint32_t s_set_rate_calls;
static uint32_t s_tree_gates;

static int prv_tree_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  s_tree_gates |= 1U << id;
  return id == TREE_GATE1 ? s_enable_ret : 0;
}

static int prv_tree_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  s_tree_gates &= ~(1U << id);
  return 0;
}

static int prv_tree_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id,
                             uint64_t parent_mhz, uint64_t *rate_mhz) {
  *rate_mhz = (id == TREE_MUX) ? parent_mhz / s_mux_div : parent_mhz;
  return 0;
}

// The mux divides by 1 to 4, after switching: its parent must be set first
static int prv_tree_set_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t rate_mhz,
                             uint64_t parent_mhz) {
  s_set_rate_calls++;
  if (id != TREE_MUX || rate_mhz == 0U || parent_mhz % rate_mhz != 0U ||
      parent_mhz / rate_mhz > 4U) {
    return -EINVAL;
  }
  s_mux_div = (uint32_t)(parent_mhz / rate_mhz);
  return 0;
}

static const struct pbl_clk_controller s_tree;

static int prv_tree_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                               struct pbl_clk *parent) {
  *parent = (id == TREE_MUX) ? (struct pbl_clk)PBL_CLK(&s_osc, s_mux_sel)
                             : (struct pbl_clk)PBL_CLK(&s_tree, TREE_MUX);
  return 0;
}

static int prv_tree_set_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                               const struct pbl_clk *parent) {
  if (id != TREE_MUX || parent->ctrl != &s_osc) {
    return -EINVAL;
  }
  if (s_set_parent_ret == 0) {
    s_mux_sel = parent->id;
  }
  return s_set_parent_ret;
}

static const struct pbl_clk_controller_ops s_tree_ops = {
  .enable = prv_tree_enable,
  .disable = prv_tree_disable,
  .get_rate = prv_tree_get_rate,
  .get_parent = prv_tree_get_parent,
  .set_parent = prv_tree_set_parent,
  .set_rate = prv_tree_set_rate,
};

PBL_CLK_CONTROLLER_STATE_DEFINE(s_tree, TREE_NUM);
static const struct pbl_clk_controller s_tree =
    PBL_CLK_CONTROLLER_INIT(s_tree, "tree", nullptr, nullptr, &s_tree_ops);

static const struct pbl_clk s_gate0 = PBL_CLK(&s_tree, TREE_GATE0);
static const struct pbl_clk s_gate1 = PBL_CLK(&s_tree, TREE_GATE1);
static const struct pbl_clk s_mux = PBL_CLK(&s_tree, TREE_MUX);

static void prv_tree_reset(void) {
  s_mux_sel = 0U;
  s_mux_div = 2U;
  s_set_rate_calls = 0U;
  s_set_parent_ret = 0;
  s_tree_gates = 0U;
  s_rates_mhz[0] = 32768000U;
  s_rates_mhz[1] = 9658250U;
  memset(s_tree_clk_use_counts, 0, sizeof(s_tree_clk_use_counts));
  memset(s_osc_clk_use_counts, 0, sizeof(s_osc_clk_use_counts));
  cl_assert_equal_i(pbl_device_init(&s_osc.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_tree.dev), 0);
}

void test_clk__tree_rate(void) {
  struct pbl_clk parent;
  uint64_t rate_mhz = 0U;

  prv_tree_reset();

  cl_assert_equal_i(pbl_clk_get_rate(&s_gate0, &rate_mhz), 0);
  cl_assert_equal_i(rate_mhz, 16384000U);

  cl_assert_equal_i(pbl_clk_get_parent(&s_gate0, &parent), 0);
  cl_assert(parent.ctrl == &s_tree && parent.id == TREE_MUX);
  cl_assert_equal_i(pbl_clk_get_parent(&s_osc0, &parent), -ENOENT);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__tree_enable(void) {
  prv_tree_reset();

  cl_assert_equal_i(pbl_clk_enable(&s_gate0), 0);
  cl_assert_equal_i(pbl_clk_enable(&s_gate1), 0);
  cl_assert_equal_i(s_tree_gates, 0x7);
  cl_assert_equal_i(s_tree_clk_use_counts[TREE_MUX], 2);
  cl_assert(pbl_clk_is_enabled(&s_osc0));

  cl_assert_equal_i(pbl_clk_disable(&s_gate0), 0);
  cl_assert(pbl_clk_is_enabled(&s_mux));
  cl_assert_equal_i(pbl_clk_disable(&s_gate1), 0);
  cl_assert_equal_i(s_tree_gates, 0x0);
  cl_assert(!pbl_clk_is_enabled(&s_mux));
  cl_assert(!pbl_clk_is_enabled(&s_osc0));
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__tree_enable_failure(void) {
  prv_tree_reset();

  s_enable_ret = -EIO;
  cl_assert_equal_i(pbl_clk_enable(&s_gate1), -EIO);
  cl_assert(!pbl_clk_is_enabled(&s_gate1));
  cl_assert(!pbl_clk_is_enabled(&s_mux));
  cl_assert(!pbl_clk_is_enabled(&s_osc0));
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__set_parent(void) {
  uint64_t rate_mhz = 0U;

  prv_tree_reset();

  cl_assert_equal_i(pbl_clk_enable(&s_gate0), 0);
  cl_assert_equal_i(pbl_clk_set_parent(&s_mux, &s_osc1), 0);
  cl_assert(!pbl_clk_is_enabled(&s_osc0));
  cl_assert(pbl_clk_is_enabled(&s_osc1));
  cl_assert_equal_i(pbl_clk_get_rate(&s_gate0, &rate_mhz), 0);
  cl_assert_equal_i(rate_mhz, 4829125U);

  s_set_parent_ret = -EIO;
  cl_assert_equal_i(pbl_clk_set_parent(&s_mux, &s_osc0), -EIO);
  cl_assert(!pbl_clk_is_enabled(&s_osc0));
  cl_assert(pbl_clk_is_enabled(&s_osc1));

  cl_assert_equal_i(pbl_clk_set_parent(&s_gate0, &s_osc0), -EINVAL);
  cl_assert_equal_i(pbl_clk_set_parent(&s_osc0, &s_osc1), -ENOTSUP);
  cl_assert_equal_i(s_lock_depth, 0);
}

static int s_tree_calls;
static uint64_t s_tree_last_mhz;

static void prv_tree_rate_changed(struct pbl_clk_notifier *nb, uint64_t rate_mhz) {
  s_tree_calls++;
  s_tree_last_mhz = rate_mhz;
}

static struct pbl_clk_notifier s_gate_nb = {
  .clk = PBL_CLK(&s_tree, TREE_GATE1),
  .fn = prv_tree_rate_changed,
};

void test_clk__notifier_tree(void) {
  static bool registered;

  prv_tree_reset();
  if (!registered) {
    pbl_clk_notifier_register(&s_gate_nb);
    registered = true;
  }
  s_tree_calls = 0;

  s_rates_mhz[0] = 32000000U;
  pbl_clk_rate_changed(&s_osc0);
  cl_assert_equal_i(s_tree_calls, 1);
  cl_assert_equal_i(s_tree_last_mhz, 16000000U);

  pbl_clk_rate_changed(&s_osc1);
  cl_assert_equal_i(s_tree_calls, 1);

  cl_assert_equal_i(pbl_clk_set_parent(&s_mux, &s_osc1), 0);
  cl_assert_equal_i(s_tree_calls, 2);
  cl_assert_equal_i(s_tree_last_mhz, 4829125U);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_clk__set_rate(void) {
  uint64_t rate_mhz = 0U;
  static struct pbl_clk_notifier nb = {.clk = PBL_CLK(&s_tree, TREE_GATE0)};
  static bool registered;

  prv_tree_reset();
  nb.fn = prv_tree_rate_changed;
  if (!registered) {
    pbl_clk_notifier_register(&nb);
    registered = true;
  }
  s_tree_calls = 0;

  cl_assert_equal_i(pbl_clk_set_rate(&s_mux, 8192000U), 0);
  cl_assert_equal_i(pbl_clk_get_rate(&s_gate0, &rate_mhz), 0);
  cl_assert_equal_i(rate_mhz, 8192000U);
  cl_assert(s_tree_calls >= 1);
  cl_assert_equal_i(s_tree_last_mhz, 8192000U);

  cl_assert_equal_i(pbl_clk_set_rate(&s_mux, 1000U), -EINVAL);
  cl_assert_equal_i(pbl_clk_set_rate(&s_osc0, 1000U), -ENOTSUP);
  cl_assert_equal_i(s_lock_depth, 0);
}

PBL_CLK_ASSIGNED_DEFINE(s_assigned,
                        PBL_CLK_ASSIGNED(PBL_CLK(&s_tree, TREE_MUX), PBL_CLK(&s_osc, 1), 0));
PBL_CLK_ASSIGNED_DEFINE(s_assigned_bad,
                        PBL_CLK_ASSIGNED(PBL_CLK(&s_tree, TREE_GATE0), PBL_CLK(&s_osc, 1), 0));
PBL_CLK_ASSIGNED_DEFINE(s_assigned_none);
// The rate comes first in the list but is set after the parent: 9658250 mHz / 1
PBL_CLK_ASSIGNED_DEFINE(s_assigned_rate, PBL_CLK_ASSIGNED(PBL_CLK(&s_tree, TREE_MUX), {}, 9658),
                        PBL_CLK_ASSIGNED(PBL_CLK(&s_tree, TREE_MUX), PBL_CLK(&s_osc, 1), 0));

void test_clk__assign(void) {
  prv_tree_reset();

  cl_assert_equal_i(pbl_clk_assign(s_assigned_none), 0);
  cl_assert_equal_i(s_mux_sel, 0U);
  cl_assert_equal_i(pbl_clk_assign(s_assigned), 0);
  cl_assert_equal_i(s_mux_sel, 1U);
  cl_assert_equal_i(s_set_rate_calls, 0U);
  cl_assert_equal_i(pbl_clk_assign(s_assigned_bad), -EINVAL);
}

void test_clk__assign_rate_after_parent(void) {
  prv_tree_reset();
  s_rates_mhz[1] = 9658000U;

  cl_assert_equal_i(pbl_clk_assign(s_assigned_rate), 0);
  cl_assert_equal_i(s_mux_sel, 1U);
  cl_assert_equal_i(s_mux_div, 1U);
}

static int prv_self_init(const struct pbl_device *dev);

static const struct pbl_clk_controller_ops s_self_ops = {
  .get_rate = prv_get_rate,
};

PBL_CLK_CONTROLLER_STATE_DEFINE(s_self, 2);
static const struct pbl_clk_controller s_self =
    PBL_CLK_CONTROLLER_INIT(s_self, "self", prv_self_init, nullptr, &s_self_ops);

static uint64_t s_self_mhz;

static int prv_self_init(const struct pbl_device *dev) {
  const struct pbl_clk clk = PBL_CLK(&s_self, 1);
  int ret;

  ret = pbl_clk_enable(&clk);
  if (ret == 0) {
    ret = pbl_clk_get_rate(&clk, &s_self_mhz);
  }
  return ret;
}

void test_clk__own_clocks_while_coming_up(void) {
  cl_assert_equal_i(pbl_device_init(&s_self.dev), 0);
  cl_assert_equal_i(s_self_mhz, s_rates_mhz[1]);
  cl_assert(pbl_clk_is_enabled(&(struct pbl_clk)PBL_CLK(&s_self, 1)));
}
