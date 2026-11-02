/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/retained.h>
#include <pbl/kernel/assert.h>

#define NUM_REGS 16U

static uint32_t s_regs[NUM_REGS];

uint32_t pbl_retained_read(uint32_t reg) {
  PBL_ASSERTN(reg < NUM_REGS);
  return s_regs[reg];
}

void pbl_retained_write(uint32_t reg, uint32_t value) {
  PBL_ASSERTN(reg < NUM_REGS);
  s_regs[reg] = value;
}
