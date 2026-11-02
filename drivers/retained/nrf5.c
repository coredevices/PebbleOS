/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/crc/crc.h>
#include <pbl/drivers/retained.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>

/* The bootloader checks the registers against a CRC32 stored right after them */
#define NUM_REGS  31U
#define NUM_WORDS 64U

static uint32_t PBL_SECTION(".retained") s_retained[NUM_WORDS];

uint32_t pbl_retained_read(uint32_t reg) {
  PBL_ASSERTN(reg < NUM_REGS);
  return s_retained[reg];
}

void pbl_retained_write(uint32_t reg, uint32_t value) {
  PBL_ASSERTN(reg < NUM_REGS);
  s_retained[reg] = value;
  s_retained[NUM_REGS] = pbl_crc32(0, s_retained, NUM_REGS * sizeof(uint32_t));
}
