/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/sf32lb52/hxt.h>
#include <pbl/kernel/section.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <bf0_hal.h>

PBL_LOG_MODULE_DEFINE(driver_sf32lb52_hxt, CONFIG_DRIVER_SF32LB52_HXT_LOG_LEVEL);

#define CFG_MAX_SIZE    256U
#define CFG_HDR_SIZE    2U
#define CFG_ID_CRYSTAL  3U
#define CFG_ID_UNINIT   0xFFU
#define CBANK_INVALID   0x3FFU
#define CBANK_SETTLE_US 40U

// Finds a record the way the boot code of the SoC vendor does, returning its length or 0
static size_t prv_find_record(const uint8_t *cfg, size_t size, uint8_t id, uint8_t *out,
                              size_t out_size) {
  size_t i = 0U;

  while (i + 1U < size && cfg[i] != CFG_ID_UNINIT) {
    size_t len = cfg[i + 1U];

    if (cfg[i] == id) {
      len = MIN(len, MIN(out_size, size - (i + CFG_HDR_SIZE)));
      memcpy(out, &cfg[i + CFG_HDR_SIZE], len);
      return len;
    }

    if (i + len + CFG_HDR_SIZE >= size) {
      return 0U;
    }

    i += len + CFG_HDR_SIZE;
  }

  return 0U;
}

// The system clock leaves the crystal while it changes, and the PLLs relock to it after
static PBL_SECTION_RAM void prv_apply_cbank(uint32_t cbank) {
  uint32_t primask;
  int clk_src;
  uint32_t dll1_hz;
  uint32_t dll2_hz;

  __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask)::"memory");

  clk_src = HAL_RCC_HCPU_GetClockSrc(RCC_CLK_MOD_SYS);
  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, RCC_SYSCLK_HRC48);
  dll1_hz = HAL_RCC_HCPU_GetDLL1Freq();
  dll2_hz = HAL_RCC_HCPU_GetDLL2Freq();

  HAL_PMU_SET_HXT_CBANK(cbank);

  HAL_Delay_us(0);
  HAL_Delay_us(CBANK_SETTLE_US);

  HAL_RCC_HCPU_DisableDLL1();
  HAL_RCC_HCPU_DisableDLL2();
  HAL_RCC_HCPU_EnableDLL1(dll1_hz);
  HAL_RCC_HCPU_EnableDLL2(dll2_hz);

  HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, clk_src);
  HAL_Delay_us(0);

  __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
}

int pbl_sf32lb52_hxt_init(const struct pbl_device *dev) {
  const struct pbl_sf32lb52_hxt *hxt = container_of(dev, const struct pbl_sf32lb52_hxt, dev);
  uint8_t cfg[CFG_MAX_SIZE];
  uint8_t rec[sizeof(uint32_t)];

  for (uint8_t i = 0U; i < hxt->num_cells; i++) {
    const struct pbl_flash_otp_cell *cell = pbl_flash_otp_cell_get(hxt->flash, hxt->cells[i]);
    uint32_t cbank;
    size_t len;
    int ret;

    if (cell == nullptr || cell->len > sizeof(cfg)) {
      return -EINVAL;
    }

    ret = pbl_flash_otp_read(hxt->flash, cell->offset, cfg, cell->len);
    if (ret != 0) {
      return ret;
    }

    memset(rec, 0, sizeof(rec));
    len = prv_find_record(cfg, cell->len, CFG_ID_CRYSTAL, rec, sizeof(rec));
    if (len == 0U) {
      continue;
    }

    cbank = (uint32_t)rec[0] | ((uint32_t)rec[1] << 8) | ((uint32_t)rec[2] << 16) |
            ((uint32_t)rec[3] << 24);
    if (cbank == 0U || cbank == CBANK_INVALID) {
      return 0;
    }

    PBL_LOG_DBG("Crystal trim 0x%03" PRIx32 " from %s", cbank, cell->name);
    prv_apply_cbank(cbank);

    return 0;
  }

  return 0;
}
