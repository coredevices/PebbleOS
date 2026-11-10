/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/sf32lb52/hxt.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/section.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_pmuc.h>

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

// The system clock leaves the crystal while it changes, and the DLLs relock to it after
static PBL_SECTION_RAM void prv_apply_cbank(const struct pbl_sf32lb52_hxt *hxt, uint32_t cbank) {
  const uintptr_t cr1 = hxt->base + SF32LB52_PMUC_HXT_CR1_OFFSET;
  struct pbl_clk_sf32lb52_hxt48_state state;
  unsigned int key;

  key = pbl_irq_disable_all();

  pbl_clk_sf32lb52_hxt48_suspend(&state);
  pbl_writel((pbl_readl(cr1) & ~PMUC_HXT_CR1_CBANK_SEL_MASK) |
                 PBL_FIELD_PREP(PMUC_HXT_CR1_CBANK_SEL_MASK, cbank),
             cr1);
  pbl_busy_wait_us(CBANK_SETTLE_US);
  pbl_clk_sf32lb52_hxt48_resume(&state);

  pbl_irq_restore_all(key);
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
    prv_apply_cbank(hxt, cbank);

    return 0;
  }

  return 0;
}

#include PBL_DT_INSTANCES(sifli_sf32lb52_pmuc_device)
