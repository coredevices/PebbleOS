/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>

#include <devicetree/regs.h>
#include <hardware/nrf54l_cache.h>
#include <hardware/nrf54l_ficr.h>
#include <hardware/nrf54l_oscillators.h>
#include <hardware/nrf54l_rramc.h>
#include <hardware/nrf54l_tampc.h>

// Undocumented registers of the oscillators block, written as the MDK does
#define OSCILLATORS_UNDOC_440_OFFSET 0x0440
#define OSCILLATORS_UNDOC_624_OFFSET 0x0624
#define OSCILLATORS_UNDOC_63C_OFFSET 0x063c
#define OSCILLATORS_UNDOC_640_OFFSET 0x0640
// Undocumented FICR words: one compared against for errata 32, and the part and its revision
#define FICR_UNDOC_334_OFFSET 0x0334
#define FICR_UNDOC_340_OFFSET 0x0340
#define FICR_UNDOC_344_OFFSET 0x0344
#define FICR_PART_NRF54L15    0x1cU
#define FICR_REVISION_1       0x01U
// Undocumented register of the radio, for errata 40
#define RADIO_UNDOC_7AC_OFFSET 0x07ac
// Undocumented register of the TAD, for errata 37
#define TAD_UNDOC_40C_OFFSET 0x040c

// Opens a debug signal the TAMPC holds, as when the UICR does not protect the device. A signal
// locked by the hardware stays as it is.
static void prv_debug_signal_open(uint32_t offset) {
  if ((pbl_readl(PBL_DT_REG_TAMPC + offset) & TAMPC_CTRL_LOCK) != 0U) {
    return;
  }

  pbl_writel(TAMPC_CTRL_WRITEPROTECTION_CLR | TAMPC_CTRL_KEY, PBL_DT_REG_TAMPC + offset);
  pbl_writel(TAMPC_CTRL_VALUE_HIGH | TAMPC_CTRL_KEY, PBL_DT_REG_TAMPC + offset);
}

static void prv_debug_init(void) {
  // Errata 37, ahead of the debug signals as the MDK does
  pbl_writel(1U, PBL_DT_REG_TAD + TAD_UNDOC_40C_OFFSET);

  prv_debug_signal_open(NRF54L_TAMPC_DOMAIN_DBGEN_CTRL_OFFSET);
  prv_debug_signal_open(NRF54L_TAMPC_DOMAIN_NIDEN_CTRL_OFFSET);
  prv_debug_signal_open(NRF54L_TAMPC_DOMAIN_SPIDEN_CTRL_OFFSET);
  prv_debug_signal_open(NRF54L_TAMPC_DOMAIN_SPNIDEN_CTRL_OFFSET);
  prv_debug_signal_open(NRF54L_TAMPC_AP_DBGEN_CTRL_OFFSET);
}

// The analog blocks start untrimmed: the FICR lists the registers to write and their values
static void prv_trim_init(void) {
  for (uint32_t i = 0U; i < FICR_TRIMCNF_NUM; i++) {
    uint32_t addr = pbl_readl(PBL_DT_REG_FICR + NRF54L_FICR_TRIMCNF_ADDR_OFFSET(i));

    if (addr == 0U || addr == UINT32_MAX) {
      break;
    }

    pbl_writel(pbl_readl(PBL_DT_REG_FICR + NRF54L_FICR_TRIMCNF_DATA_OFFSET(i)), addr);
  }
}

static void prv_errata_init(void) {
  if (pbl_readl(PBL_DT_REG_FICR + FICR_UNDOC_340_OFFSET) != FICR_PART_NRF54L15) {
    return;
  }

  if (pbl_readl(PBL_DT_REG_OSCILLATORS + OSCILLATORS_UNDOC_440_OFFSET) == 0U) {
    pbl_writel(0xc8U, PBL_DT_REG_OSCILLATORS + OSCILLATORS_UNDOC_440_OFFSET);
  }

  // Errata 31, 32 and 40 are of the first revision only
  if (pbl_readl(PBL_DT_REG_FICR + FICR_UNDOC_344_OFFSET) != FICR_REVISION_1) {
    return;
  }

  // Errata 32
  if (pbl_readl(PBL_DT_REG_FICR + FICR_UNDOC_334_OFFSET) <= 0x180a1d00U) {
    pbl_writel(0x1ea9e040U, PBL_DT_REG_OSCILLATORS + OSCILLATORS_UNDOC_640_OFFSET);
  }

  // Errata 40
  pbl_writel(0x040a0078U, PBL_DT_REG_RADIO + RADIO_UNDOC_7AC_OFFSET);

  // Errata 31
  pbl_writel(20U | PBL_BIT(5), PBL_DT_REG_OSCILLATORS + OSCILLATORS_UNDOC_624_OFFSET);
  pbl_modifyl(PBL_BIT(19), 0U, PBL_DT_REG_OSCILLATORS + OSCILLATORS_UNDOC_63C_OFFSET);
}

void pbl_soc_early_init(void) {
  prv_debug_init();
  prv_trim_init();
  prv_errata_init();

  pbl_writel(OSCILLATORS_PLL_FREQ_CK128M,
             PBL_DT_REG_OSCILLATORS + NRF54L_OSCILLATORS_PLL_FREQ_OFFSET);
  pbl_cycles_per_sec_set(128000000U);

  // The RRAM powers off while idle, for 9 us more to wake up
  pbl_writel(RRAMC_POWER_LOWPOWERCONFIG_POWEROFF,
             PBL_DT_REG_RRAMC + NRF54L_RRAMC_POWER_LOWPOWERCONFIG_OFFSET);
  pbl_writel(0U, PBL_DT_REG_RRAMC + NRF54L_GLITCHDET_CONFIG_OFFSET);

  pbl_writel(CACHE_ENABLE_ENABLE, PBL_DT_REG_ICACHE + NRF54L_CACHE_ENABLE_OFFSET);
}
