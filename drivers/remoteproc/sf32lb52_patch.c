/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sf32lb52_priv.h"

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_patch.h>

#ifdef CONFIG_SOC_SF32LB57
#include <lcpu_patch.h>

// LCPU ROM layout of its SRAM region: header, then code
#define PATCH_BUF_OFFSET  0x10000U
#define PATCH_CODE_OFFSET 0x1000cU
#define PATCH_CODE_SIZE   0x35f4U

#define PATCH_HDR     sf32lb57_lcpu_patch_hdr
#define PATCH_ENTRIES sf32lb57_lcpu_patch_entries
#define PATCH_CODE    sf32lb57_lcpu_patch_code
#else
#include <hardware/sf32lb52_hpsys_cfg.h>
#include <lcpu_patch_rev_b.h>
#include <revid.h>

// LCPU ROM layout of its SRAM region: header, then code
#define PATCH_BUF_OFFSET  0x5000U
#define PATCH_CODE_OFFSET 0x500cU
#define PATCH_CODE_SIZE   0x2ff4U

#define PATCH_HDR     sf32lb52_lcpu_patch_hdr
#define PATCH_ENTRIES sf32lb52_lcpu_patch_entries
#define PATCH_CODE    sf32lb52_lcpu_patch_code
#endif

static_assert(PATCH_CODE_OFFSET == PATCH_BUF_OFFSET + sizeof(PATCH_HDR) + sizeof(uint32_t));
static_assert(sizeof(PATCH_CODE) <= PATCH_CODE_SIZE);
static_assert(ARRAY_LENGTH(PATCH_ENTRIES) <= SF32LB52_PATCH_NUM_CHANNELS);

void pbl_rproc_sf32lb52__patch_install(const struct pbl_rproc_sf32lb52 *lcpu) {
  uintptr_t base = lcpu->patch_base;
  uintptr_t code = lcpu->sram.base + PATCH_CODE_OFFSET;
  uint32_t *hdr = (uint32_t *)(lcpu->sram.base + PATCH_BUF_OFFSET);
  size_t i;

  hdr[0] = PATCH_HDR[0];
  hdr[1] = PATCH_HDR[1];
#ifdef CONFIG_SOC_SF32LB57
  // The SF32LB57 ROM takes the code at its system bus address
  hdr[2] = code | 1U;
#else
  hdr[2] = pbl_rproc_sf32lb52_da(lcpu, code) | 1U;
#endif

  memset((void *)code, 0, PATCH_CODE_SIZE);
  memcpy((void *)code, PATCH_CODE, sizeof(PATCH_CODE));

#ifndef CONFIG_SOC_SF32LB57
  // Revisions before A4 do not take these entries
  if (soc_sf32lb52_revid() < HPSYS_CFG_IDR_REVID_A4) {
    return;
  }
#endif

  pbl_writel(0U, base + SF32LB52_PATCH_CER_OFFSET);

  for (i = 0U; i < ARRAY_LENGTH(PATCH_ENTRIES); i++) {
    pbl_writel(PATCH_ENTRIES[i][0] & PATCH_CH_ADDR_MASK, base + SF32LB52_PATCH_CH_OFFSET(i));
    pbl_writel(PBL_BIT(i), base + SF32LB52_PATCH_CSR_OFFSET);
    pbl_writel(PATCH_ENTRIES[i][1], base + SF32LB52_PATCH_CDR_OFFSET);
  }

  pbl_writel(0U, base + SF32LB52_PATCH_CSR_OFFSET);
  pbl_writel(PBL_GENMASK(ARRAY_LENGTH(PATCH_ENTRIES) - 1U, 0U), base + SF32LB52_PATCH_CER_OFFSET);
}
