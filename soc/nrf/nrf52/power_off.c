/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>

#include <devicetree/regs.h>
#include <hardware/nrf52_power.h>

[[noreturn]] void pbl_soc_power_off(void) {
  pbl_writel(POWER_SYSTEMOFF_SYSTEMOFF, PBL_DT_REG_CLOCK + NRF52_POWER_SYSTEMOFF_OFFSET);

  while (true) {
  }
}
