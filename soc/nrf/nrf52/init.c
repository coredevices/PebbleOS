/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/init.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>

#include <hardware/nrf52_approtect.h>
#include <hardware/nrf52_clock.h>
#include <hardware/nrf52_ficr.h>
#include <hardware/nrf52_nvmc.h>
#include <hardware/nrf52_uicr.h>

#define RESET_PIN 18U

uint32_t SystemCoreClock = 64000000U;

static void prv_uicr_write(uint32_t val, uintptr_t offset) {
  pbl_writel(NVMC_CONFIG_WEN_WEN, NRF52_NVMC_BASE + NRF52_NVMC_CONFIG_OFFSET);
  pbl_writel(val, NRF52_UICR_BASE + offset);
  while ((pbl_readl(NRF52_NVMC_BASE + NRF52_NVMC_READY_OFFSET) & NVMC_READY_READY) == 0U) {
  }
  pbl_writel(NVMC_CONFIG_WEN_REN, NRF52_NVMC_BASE + NRF52_NVMC_CONFIG_OFFSET);
}

static void prv_approtect_init(void) {
  if (pbl_readl(NRF52_FICR_BASE + NRF52_FICR_PART_OFFSET) != FICR_PART_NRF52840 ||
      pbl_readl(NRF52_FICR_BASE + NRF52_FICR_REVISION_OFFSET) < FICR_REVISION_HARDENED_APPROTECT) {
    return;
  }

  pbl_writel(pbl_readl(NRF52_UICR_BASE + NRF52_UICR_APPROTECT_OFFSET),
             NRF52_APPROTECT_BASE + NRF52_APPROTECT_DISABLE_OFFSET);
}

static bool prv_reset_pin_connected(void) {
  for (unsigned int i = 0U; i < 2U; i++) {
    if ((pbl_readl(NRF52_UICR_BASE + NRF52_UICR_PSELRESET_OFFSET(i)) &
         UICR_PSELRESET_DISCONNECTED) != 0U) {
      return false;
    }
  }

  return true;
}

static void prv_uicr_init(void) {
  uint32_t nfcpins = pbl_readl(NRF52_UICR_BASE + NRF52_UICR_NFCPINS_OFFSET);
  if ((nfcpins & UICR_NFCPINS_PROTECT_NFC) != 0U) {
    prv_uicr_write(nfcpins & ~UICR_NFCPINS_PROTECT_NFC, NRF52_UICR_NFCPINS_OFFSET);
    pbl_reboot_emergency();
  }

  if (!prv_reset_pin_connected()) {
    prv_uicr_write(RESET_PIN, NRF52_UICR_PSELRESET_OFFSET(0));
    prv_uicr_write(RESET_PIN, NRF52_UICR_PSELRESET_OFFSET(1));
    pbl_reboot_emergency();
  }
}

static void prv_lfclk_init(void) {
  pbl_writel(CLOCK_LFCLKSRC_SRC_XTAL, NRF52_CLOCK_BASE + NRF52_CLOCK_LFCLKSRC_OFFSET);
  pbl_writel(0U, NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET);
  pbl_writel(1U, NRF52_CLOCK_BASE + NRF52_CLOCK_TASKS_LFCLKSTART_OFFSET);
  // TODO: time out and fall back to the calibrated RC, which needs a runtime BLE SCA. Until
  // then, the bootloader's watchdog reboots if the crystal does not start.
  while (pbl_readl(NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET) == 0U) {
  }
  pbl_writel(0U, NRF52_CLOCK_BASE + NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET);
}

void pbl_soc_early_init(void) {
  prv_approtect_init();
  prv_uicr_init();

  prv_lfclk_init();

  pbl_writel(NVMC_ICACHECNF_CACHEEN, NRF52_NVMC_BASE + NRF52_NVMC_ICACHECNF_OFFSET);
}
