/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/intc.h>

#include <clar.h>

static int s_lock_depth;
static uint32_t s_types[4];
static uint32_t s_unmasked;
static int s_configure_ret;
static int s_enable_calls;
static int s_disable_calls;
static int s_handler_calls;
static void *s_handler_arg;

void pbl_irq_lock(void) {
  s_lock_depth++;
}

void pbl_irq_unlock(void) {
  s_lock_depth--;
}

static int prv_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  cl_assert(s_lock_depth > 0);
  cl_assert((s_unmasked & (1U << hwirq)) == 0U);
  if (s_configure_ret == 0) {
    s_types[hwirq] = type;
  }
  return s_configure_ret;
}

static int prv_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  cl_assert(s_lock_depth > 0);
  s_enable_calls++;
  s_unmasked |= 1U << hwirq;
  return 0;
}

static int prv_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  cl_assert(s_lock_depth > 0);
  s_disable_calls++;
  s_unmasked &= ~(1U << hwirq);
  return 0;
}

static const struct pbl_intc_ops s_ops = {
  .configure = prv_configure,
  .enable = prv_enable,
  .disable = prv_disable,
};

PBL_INTC_STATE_DEFINE(s_intc, 4);
static const struct pbl_intc s_intc = PBL_INTC_INIT(s_intc, "intc", nullptr, nullptr, &s_ops);
PBL_DEVICE_REGISTER(s_intc, &s_intc.dev);

static const struct pbl_irq_spec s_line1 = PBL_IRQ_SPEC(&s_intc, 1, PBL_IRQ_TYPE_EDGE_FALLING);
static const struct pbl_irq_spec s_line3 = PBL_IRQ_SPEC(&s_intc, 3, PBL_IRQ_TYPE_LEVEL_LOW);
static const struct pbl_irq_spec s_line_bad = PBL_IRQ_SPEC(&s_intc, 4, PBL_IRQ_TYPE_EDGE_RISING);

static void prv_handler(void *arg) {
  s_handler_calls++;
  s_handler_arg = arg;
}

void test_intc__initialize(void) {
  s_lock_depth = 0;
  memset(s_types, 0, sizeof(s_types));
  s_unmasked = 0U;
  s_configure_ret = 0;
  s_enable_calls = 0;
  s_disable_calls = 0;
  s_handler_calls = 0;
  s_handler_arg = nullptr;
  memset(s_intc_intc_lines, 0, sizeof(s_intc_intc_lines));
  cl_assert_equal_i(pbl_device_init(&s_intc.dev), 0);
}

void test_intc__num_irqs(void) {
  cl_assert_equal_i(s_intc.num_irqs, 4);
}

void test_intc__request(void) {
  int arg;

  cl_assert_equal_i(pbl_intc_request(&s_line1, prv_handler, &arg), 0);
  cl_assert_equal_i(s_types[1], PBL_IRQ_TYPE_EDGE_FALLING);
  cl_assert_equal_i(s_unmasked, 0x0);
  cl_assert(!s_intc.lines[1].enabled);

  pbl_intc_dispatch(&s_intc, 1);
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_p(s_handler_arg, &arg);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_intc__request_busy(void) {
  cl_assert_equal_i(pbl_intc_request(&s_line1, prv_handler, nullptr), 0);
  cl_assert_equal_i(pbl_intc_request(&s_line1, prv_handler, nullptr), -EBUSY);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_intc__request_invalid(void) {
  const struct pbl_irq_spec no_type = PBL_IRQ_SPEC(&s_intc, 1, 0);
  const struct pbl_irq_spec mixed =
      PBL_IRQ_SPEC(&s_intc, 1, PBL_IRQ_TYPE_EDGE_RISING | PBL_IRQ_TYPE_LEVEL_HIGH);

  cl_assert_equal_i(pbl_intc_request(&s_line_bad, prv_handler, nullptr), -EINVAL);
  cl_assert_equal_i(pbl_intc_request(&no_type, prv_handler, nullptr), -EINVAL);
  cl_assert_equal_i(pbl_intc_request(&mixed, prv_handler, nullptr), -EINVAL);
  cl_assert_equal_i(pbl_intc_request(&s_line1, nullptr, nullptr), -EINVAL);
}

void test_intc__request_unsupported(void) {
  s_configure_ret = -ENOTSUP;
  cl_assert_equal_i(pbl_intc_request(&s_line3, prv_handler, nullptr), -ENOTSUP);
  cl_assert(s_intc.lines[3].handler == nullptr);

  s_configure_ret = 0;
  cl_assert_equal_i(pbl_intc_request(&s_line3, prv_handler, nullptr), 0);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_intc__enable_disable(void) {
  cl_assert_equal_i(pbl_intc_request(&s_line1, prv_handler, nullptr), 0);
  cl_assert_equal_i(pbl_intc_request(&s_line3, prv_handler, nullptr), 0);

  cl_assert_equal_i(pbl_intc_enable(&s_line1), 0);
  cl_assert_equal_i(pbl_intc_enable(&s_line1), 0);
  cl_assert_equal_i(s_enable_calls, 1);
  cl_assert_equal_i(s_unmasked, 0x2);

  cl_assert_equal_i(pbl_intc_enable(&s_line3), 0);
  cl_assert_equal_i(s_unmasked, 0xa);

  cl_assert_equal_i(pbl_intc_disable(&s_line1), 0);
  cl_assert_equal_i(pbl_intc_disable(&s_line1), 0);
  cl_assert_equal_i(s_disable_calls, 1);
  cl_assert_equal_i(s_unmasked, 0x8);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_intc__not_requested(void) {
  cl_assert_equal_i(pbl_intc_enable(&s_line1), -EINVAL);
  cl_assert_equal_i(pbl_intc_disable(&s_line1), -EINVAL);
  cl_assert_equal_i(pbl_intc_free(&s_line1), -EINVAL);
  cl_assert_equal_i(pbl_intc_enable(&s_line_bad), -EINVAL);
  cl_assert_equal_i(s_enable_calls, 0);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_intc__free(void) {
  cl_assert_equal_i(pbl_intc_request(&s_line1, prv_handler, nullptr), 0);
  cl_assert_equal_i(pbl_intc_enable(&s_line1), 0);

  cl_assert_equal_i(pbl_intc_free(&s_line1), 0);
  cl_assert_equal_i(s_unmasked, 0x0);
  cl_assert(s_intc.lines[1].handler == nullptr);

  cl_assert_equal_i(pbl_intc_request(&s_line1, prv_handler, nullptr), 0);
  cl_assert(!s_intc.lines[1].enabled);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_intc__dispatch_spurious(void) {
  s_unmasked = 0x2;

  pbl_intc_dispatch(&s_intc, 1);
  cl_assert_equal_i(s_handler_calls, 0);
  cl_assert_equal_i(s_unmasked, 0x0);

  pbl_intc_dispatch(&s_intc, 4);
  cl_assert_equal_i(s_disable_calls, 1);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_intc__spec_connected(void) {
  const struct pbl_irq_spec none = {0};

  cl_assert(pbl_irq_spec_is_connected(&s_line1));
  cl_assert(!pbl_irq_spec_is_connected(&none));
}
