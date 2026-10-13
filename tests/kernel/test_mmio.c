/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/barrier.h>
#include <pbl/kernel/mmio.h>

#include <clar.h>

static union {
  uint32_t l;
  uint16_t w[2];
  uint8_t b[4];
} s_reg;

void test_mmio__initialize(void) {
  s_reg.l = 0U;
}

void test_mmio__widths(void) {
  pbl_writel(0x11223344U, (uintptr_t)&s_reg.l);
  cl_assert_equal_i(pbl_readl((uintptr_t)&s_reg.l), 0x11223344U);
  cl_assert_equal_i(pbl_readw((uintptr_t)&s_reg.w[0]), 0x3344U);
  cl_assert_equal_i(pbl_readb((uintptr_t)&s_reg.b[0]), 0x44U);

  pbl_writew(0xaabbU, (uintptr_t)&s_reg.w[1]);
  cl_assert_equal_i(pbl_readl((uintptr_t)&s_reg.l), 0xaabb3344U);

  pbl_writeb(0xccU, (uintptr_t)&s_reg.b[1]);
  cl_assert_equal_i(pbl_readl((uintptr_t)&s_reg.l), 0xaabbcc44U);
}

void test_mmio__dma_handoff(void) {
  static uint32_t s_buf[2];

  s_buf[0] = 0x5aU;
  pbl_wmb();
  pbl_writel(1U, (uintptr_t)&s_reg.l);

  cl_assert_equal_i(pbl_readl((uintptr_t)&s_reg.l), 1U);
  pbl_rmb();
  cl_assert_equal_i(s_buf[0], 0x5aU);
}
