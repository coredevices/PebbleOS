/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ram/stm32_fmc.h>

// The -6 speed grade, the slowest timings of those the part comes in at CAS latency 3
const struct pbl_ram_sdram_part pbl_ram_sdram_is42s32800j = {
  .name = "IS42S32800J",
  .banks = 4U,
  .row_bits = 12U,
  .col_bits = 9U,
  .width = 32U,
  .cas_latency = 3U,
  .t_mrd_ck = 2U,
  .t_wr_ck = 2U,
  .t_xsr_ns = 70U,
  .t_ras_ns = 42U,
  .t_rc_ns = 60U,
  .t_rp_ns = 18U,
  .t_rcd_ns = 18U,
  .refresh_ms = 64U,
};
