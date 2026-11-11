/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sf32lb52_priv.h"

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_patch.h>
#include <lcpu_patch_rev_b.h>
#include <revid.h>

// LCPU ROM layout of its SRAM region: header, then code
#define PATCH_BUF_OFFSET  0x5000U
#define PATCH_CODE_OFFSET 0x500cU
#define PATCH_CODE_SIZE   0x2ff4U

// The LCPU code bus sees LPSYS SRAM this far below the system bus
#define LCPU_CBUS_OFFSET 0x20000000UL

static_assert(PATCH_CODE_OFFSET ==
              PATCH_BUF_OFFSET + sizeof(sf32lb52_lcpu_patch_hdr) + sizeof(uint32_t));
static_assert(sizeof(sf32lb52_lcpu_patch_code) <= PATCH_CODE_SIZE);
static_assert(ARRAY_LENGTH(sf32lb52_lcpu_patch_entries) <= SF32LB52_PATCH_NUM_CHANNELS);

void pbl_rproc_sf32lb52__patch_install(const struct pbl_rproc_sf32lb52 *lcpu) {
  uintptr_t base = lcpu->patch_base;
  uintptr_t code = lcpu->sram.base + PATCH_CODE_OFFSET;
  uint32_t *hdr = (uint32_t *)(lcpu->sram.base + PATCH_BUF_OFFSET);
  uint32_t rev;
  size_t i;

  hdr[0] = sf32lb52_lcpu_patch_hdr[0];
  hdr[1] = sf32lb52_lcpu_patch_hdr[1];
  hdr[2] = (code - LCPU_CBUS_OFFSET) | 1U;

  memset((void *)code, 0, PATCH_CODE_SIZE);
  memcpy((void *)code, sf32lb52_lcpu_patch_code, sizeof(sf32lb52_lcpu_patch_code));

  // Revisions before A4 do not take these entries
  rev = soc_sf32lb52_revid();
  if (rev < HPSYS_CFG_IDR_REVID_A4) {
    return;
  }

  pbl_writel(0U, base + SF32LB52_PATCH_CER_OFFSET);

  for (i = 0U; i < ARRAY_LENGTH(sf32lb52_lcpu_patch_entries); i++) {
    pbl_writel(sf32lb52_lcpu_patch_entries[i][0] & PATCH_CH_ADDR_MASK,
               base + SF32LB52_PATCH_CH_OFFSET(i));
    pbl_writel(PBL_BIT(i), base + SF32LB52_PATCH_CSR_OFFSET);
    pbl_writel(sf32lb52_lcpu_patch_entries[i][1], base + SF32LB52_PATCH_CDR_OFFSET);
  }

  pbl_writel(0U, base + SF32LB52_PATCH_CSR_OFFSET);
  pbl_writel(PBL_GENMASK(ARRAY_LENGTH(sf32lb52_lcpu_patch_entries) - 1U, 0U),
             base + SF32LB52_PATCH_CER_OFFSET);
}
