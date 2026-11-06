/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/nvmem/sf32lb52.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>

#include <hardware/sf32lb52_efusec.h>
#include <hardware/sf32lb52_pmuc.h>

#define EFUSE_WORD_SIZE 4U

#define RD_STROBE_NS    500U
#define PCLK_MAX_HZ     120000000U
#define READ_TIMEOUT_US 2000U

// Reads need the HPSYS LDO raised
#define VOUT_BOOST     3U
#define VOUT_READ_MIN  0xeU
#define VOUT_READ_MAX  0xfU
#define VOUT_SETTLE_US 20U

static const struct pbl_clk s_clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_EFUSEC);

static int prv_set_timing(void) {
  uint64_t rate_mhz;
  uint32_t pclk_hz;
  uint32_t thrck;
  int ret;

  ret = pbl_clk_get_rate(&s_clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  pclk_hz = (uint32_t)(rate_mhz / 1000U);
  if (pclk_hz > PCLK_MAX_HZ) {
    return -ERANGE;
  }

  thrck = (uint32_t)(((uint64_t)RD_STROBE_NS * pclk_hz) / 1000000000U) + 1U;
  pbl_modifyl(EFUSEC_TIMR_THRCK_MASK, PBL_FIELD_PREP(EFUSEC_TIMR_THRCK_MASK, thrck),
              SF32LB52_EFUSEC_BASE + SF32LB52_EFUSEC_TIMR_OFFSET);

  return 0;
}

static int prv_read_bank(unsigned int bank, unsigned int offset, uint8_t *val, size_t bytes) {
  const uintptr_t data = SF32LB52_EFUSEC_BASE + SF32LB52_EFUSEC_BANK_DATA_OFFSET(bank);
  const uintptr_t sr = SF32LB52_EFUSEC_BASE + SF32LB52_EFUSEC_SR_OFFSET;
  const uintptr_t cr = SF32LB52_EFUSEC_BASE + SF32LB52_EFUSEC_CR_OFFSET;
  const uintptr_t vout = SF32LB52_PMUC_BASE + SF32LB52_PMUC_HPSYS_VOUT_OFFSET;
  const uint32_t vout_saved = pbl_readl(vout);
  uint32_t vout_read = vout_saved + VOUT_BOOST;
  int ret;

  if (vout_read < VOUT_READ_MIN) {
    vout_read = VOUT_READ_MIN;
  } else if (vout_read > VOUT_READ_MAX) {
    vout_read = VOUT_READ_MAX;
  }

  pbl_writel(vout_read, vout);
  pbl_busy_wait_us(VOUT_SETTLE_US);

  pbl_writel(PBL_FIELD_PREP(EFUSEC_CR_BANKSEL_MASK, bank), cr);
  pbl_modifyl(0U, EFUSEC_CR_EN, cr);

  ret = PBL_POLL_BUSY_US((pbl_readl(sr) & EFUSEC_SR_DONE) != 0U, 1U, READ_TIMEOUT_US);
  pbl_modifyl(0U, EFUSEC_SR_DONE, sr);

  if (ret == 0) {
    for (size_t i = 0U; i < bytes; i += EFUSE_WORD_SIZE) {
      uint32_t word = pbl_readl(data + offset + i);

      memcpy(&val[i], &word, sizeof(word));
    }
  }

  pbl_writel(vout_saved, vout);

  return ret;
}

static int prv_read_locked(unsigned int offset, uint8_t *val, size_t bytes) {
  int ret;

  ret = prv_set_timing();
  if (ret != 0) {
    return ret;
  }

  while (bytes > 0U) {
    const unsigned int bank_offset = offset % SF32LB52_EFUSEC_BANK_SIZE;
    const size_t len = MIN(bytes, SF32LB52_EFUSEC_BANK_SIZE - bank_offset);

    ret = prv_read_bank(offset / SF32LB52_EFUSEC_BANK_SIZE, bank_offset, val, len);
    if (ret != 0) {
      return ret;
    }

    offset += len;
    val += len;
    bytes -= len;
  }

  return 0;
}

static int prv_reg_read(const struct pbl_nvmem *nvmem, unsigned int offset, void *val,
                        size_t bytes) {
  int ret;

  ret = pbl_clk_enable(&s_clk);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_lock();
  ret = prv_read_locked(offset, val, bytes);
  pbl_irq_unlock();

  (void)pbl_clk_disable(&s_clk);

  return ret;
}

static const struct pbl_nvmem_ops s_ops = {
  .reg_read = prv_reg_read,
};

PBL_DEVICE_STATE_DEFINE(pbl_nvmem_sf32lb52_efuse);
const struct pbl_nvmem pbl_nvmem_sf32lb52_efuse = PBL_NVMEM_INIT(
    pbl_nvmem_sf32lb52_efuse, "efuse", nullptr, PBL_DEVICE_DEPS(&SF32LB_HPSYS_RCC->dev), &s_ops,
    SF32LB52_EFUSEC_BANK_SIZE *SF32LB52_EFUSEC_NUM_BANKS, EFUSE_WORD_SIZE, EFUSE_WORD_SIZE);
PBL_DEVICE_REGISTER(pbl_nvmem_sf32lb52_efuse, &pbl_nvmem_sf32lb52_efuse.dev);
