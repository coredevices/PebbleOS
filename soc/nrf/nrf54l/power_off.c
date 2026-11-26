/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>

#include <devicetree/regs.h>
#include <hardware/nrf54l_clock.h>
#include <hardware/nrf54l_memconf.h>
#include <hardware/nrf54l_oscillators.h>

[[noreturn]] void pbl_soc_power_off(void) {
  // Waking from System OFF resets the SoC: no RAM, the RISC-V core's included, is kept
  for (uint32_t i = 0U; i < NRF54L_MEMCONF_POWER_COUNT; i++) {
    pbl_writel(0U, PBL_DT_REG_MEMCONF + NRF54L_MEMCONF_POWER_RET_OFFSET(i));
#ifdef CONFIG_SOC_NRF54L15
    pbl_writel(0U, PBL_DT_REG_MEMCONF + NRF54L_MEMCONF_POWER_RET2_OFFSET(i));
#endif
  }

  // The reasons are cleared so that the next boot sees the wake-up alone
  pbl_writel(UINT32_MAX, PBL_DT_REG_CLOCK + NRF54L_RESET_RESETREAS_OFFSET);

  pbl_writel(REGULATORS_SYSTEMOFF_SYSTEMOFF,
             PBL_DT_REG_OSCILLATORS + NRF54L_REGULATORS_SYSTEMOFF_OFFSET);

  while (true) {
  }
}
