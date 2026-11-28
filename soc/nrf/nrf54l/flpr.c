/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/init.h>
#include <pbl/kernel/time.h>

#include <soc_riscv.h>

// The application core set up the supply, the clocks and the trims before starting this one
void pbl_soc_early_init(void) {
  pbl_nrf_vpr_init();
  pbl_cycles_per_sec_set(128000000U);
}
