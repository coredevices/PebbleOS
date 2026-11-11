/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mmio.h>

#include <devicetree/regs.h>
#include <hardware/sf32lb52_hpsys_cfg.h>

// Chip revision, HPSYS_CFG_IDR_REVID_*. Inline, for code running with the flash down.
static PBL_ALWAYS_INLINE uint32_t soc_sf32lb52_revid(void) {
  return pbl_readl(PBL_DT_REG_HPSYS_CFG + SF32LB52_HPSYS_CFG_IDR_OFFSET) & HPSYS_CFG_IDR_REVID_MASK;
}
