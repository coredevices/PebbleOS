/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/retained.h>
#include <pbl/kernel/assert.h>

static uint32_t s_regs[PBL_RETAINED_COUNT];

uint32_t pbl_retained_read(enum pbl_retained_reg reg) {
  PBL_ASSERTN(reg < PBL_RETAINED_COUNT);
  return s_regs[reg];
}

void pbl_retained_write(enum pbl_retained_reg reg, uint32_t value) {
  PBL_ASSERTN(reg < PBL_RETAINED_COUNT);
  s_regs[reg] = value;
}
