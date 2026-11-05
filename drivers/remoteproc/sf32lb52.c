/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/remoteproc/sf32lb52.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <bf0_hal.h>

// LPSYS RAM areas used by the LCPU ROM
#define LCPU_CODE_BASE  0x20400000UL
#define LCPU_PATCH_SIZE 0x500U
#define LCPU_EM_BASE    0x20408000UL
#define LCPU_EM_SIZE    0x5000U

#define LCPU_LOAD_HCLK_MAX_HZ 24000000UL
#define LCPU_BOOT_DELAY_US    5000U

extern void lcpu_patch_install_rev_b(void);
extern void bt_rf_cal(void);

static const struct pbl_rproc_sf32lb52 *prv_lcpu(const struct pbl_rproc *rproc) {
  return container_of(rproc, const struct pbl_rproc_sf32lb52, rproc);
}

static void prv_rom_config(const struct pbl_rproc_sf32lb52 *lcpu) {
  uint8_t xtal_enabled = HAL_LXT_DISABLED() ? 0U : 1U;
  uint8_t rc_cal = 1U - xtal_enabled;
  uint32_t wdt_status = 0xffU;
  uint32_t wdt_time = 10U;
  uint16_t wdt_clk = 32768U;
  uint32_t tx_queue = lcpu->mem.base;
  hal_lcpu_bluetooth_rom_config_t bt = {
    .bit_valid = PBL_BIT(10) | PBL_BIT(6) | PBL_BIT(2),
    .lld_prog_delay = 3U,
    .default_xtal_enabled = xtal_enabled,
    .is_fpga = 0U,
  };
  hal_lcpu_ble_mem_config_t ble = {
    .bit_valid = PBL_BIT(6),
    .max_nb_of_hci_completed = 6U,
  };

  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_XTAL_ENABLED, &xtal_enabled, sizeof(xtal_enabled));
  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_WDT_STATUS, &wdt_status, sizeof(wdt_status));
  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_WDT_TIME, &wdt_time, sizeof(wdt_time));
  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_WDT_CLK_FEQ, &wdt_clk, sizeof(wdt_clk));
  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_BT_RC_CAL_IN_L, &rc_cal, sizeof(rc_cal));
  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_HCPU_TX_QUEUE, &tx_queue, sizeof(tx_queue));
  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_BT_CONFIG, &bt, sizeof(bt));
  HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_BT_KE_BUF, &ble, sizeof(ble));
}

static int prv_start(const struct pbl_rproc *rproc) {
  HAL_HPAON_WakeCore(CORE_ID_LCPU);
  HAL_RCC_Reset_and_Halt_LCPU(0);

  prv_rom_config(prv_lcpu(rproc));

  if (HAL_RCC_GetHCLKFreq(CORE_ID_LCPU) > LCPU_LOAD_HCLK_MAX_HZ) {
    HAL_RCC_LCPU_SetDiv(2, 1, 5);
  }

  HAL_LPAON_ConfigStartAddr((uint32_t *)LCPU_CODE_BASE);

  memset((void *)LCPU_CODE_BASE, 0, LCPU_PATCH_SIZE);
  lcpu_patch_install_rev_b();
  bt_rf_cal();
  // calibration leaves data in the exchange memory
  memset((void *)LCPU_EM_BASE, 0, LCPU_EM_SIZE);

  HAL_RCC_ReleaseLCPU();
  HAL_HPAON_CANCEL_LP_ACTIVE_REQUEST();

  pbl_busy_wait_us(LCPU_BOOT_DELAY_US);

  return 0;
}

static void prv_stop(const struct pbl_rproc *rproc) {
  // LPSYS_AON is only reachable while the wake request is held
  HAL_HPAON_WakeCore(CORE_ID_LCPU);
  HAL_RCC_Reset_and_Halt_LCPU(0);
  HAL_HPAON_CANCEL_LP_ACTIVE_REQUEST();
}

const struct pbl_rproc_ops pbl_rproc_sf32lb52_ops = {
  .start = prv_start,
  .stop = prv_stop,
};

int pbl_rproc_sf32lb52_init(const struct pbl_device *dev) {
  const struct pbl_rproc_sf32lb52 *lcpu =
      container_of(dev, const struct pbl_rproc_sf32lb52, rproc.dev);
  int ret;

  if (lcpu->mem.size == 0U) {
    return -EINVAL;
  }

  ret = pbl_device_init(&lcpu->tx_chan.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_device_init(&lcpu->rx_chan.ctrl->dev);
}
