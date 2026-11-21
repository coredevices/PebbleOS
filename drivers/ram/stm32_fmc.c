/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/ram/stm32_fmc.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_fmc.h>

#define SDCLK_MAX_HZ 100000000U

// The SDRAM wants 100 us of clock before its first command
#define POWER_UP_US 100U

#define CMD_TIMEOUT_US 1000U

// Auto-refresh commands in the power-up sequence
#define POWER_UP_REFRESHES 8U

// The refresh counter starts that many cycles early, to leave room for a pending access
#define REFRESH_MARGIN 20U

// Mode register: burst of 1, sequential, CAS latency, single writes
#define MODE_CAS_POS      4U
#define MODE_SINGLE_WRITE PBL_BIT(9)

static const struct pbl_ram_stm32_fmc *prv_fmc(const struct pbl_ram *ram) {
  return PBL_CONTAINER_OF(ram, const struct pbl_ram_stm32_fmc, ram);
}

static uint32_t prv_cycles(uint32_t ns, uint32_t hz) {
  return (uint32_t)(((uint64_t)ns * hz + 999999999U) / 1000000000U);
}

static int prv_cmd(const struct pbl_ram_stm32_fmc *fmc, uint32_t ctb, uint32_t mode, uint32_t nrfs,
                   uint32_t mrd) {
  int ret = PBL_POLL_BUSY_US((pbl_readl(fmc->base + STM32H7_FMC_SDSR_OFFSET) & FMC_SDSR_BUSY) == 0U,
                             1U, CMD_TIMEOUT_US);

  if (ret != 0) {
    return ret;
  }

  pbl_writel(ctb | PBL_FIELD_PREP(FMC_SDCMR_MODE_MASK, mode) |
                 PBL_FIELD_PREP(FMC_SDCMR_NRFS_MASK, nrfs) |
                 PBL_FIELD_PREP(FMC_SDCMR_MRD_MASK, mrd),
             fmc->base + STM32H7_FMC_SDCMR_OFFSET);

  return 0;
}

// The FMC fields of a timing hold one less than the cycles, from 1 to 16
static uint32_t prv_timing(uint32_t cycles, uint32_t mask) {
  return PBL_FIELD_PREP(mask, PBL_MAX(cycles, 1U) - 1U);
}

static int prv_configure(const struct pbl_ram_stm32_fmc *fmc, uint32_t bank, uint32_t sd_hz,
                         uint32_t sdclk) {
  const struct pbl_ram_sdram_part *part = fmc->part;
  uint32_t ras = prv_cycles(part->t_ras_ns, sd_hz);
  uint32_t rc = prv_cycles(part->t_rc_ns, sd_hz);
  uint32_t rp = prv_cycles(part->t_rp_ns, sd_hz);
  uint32_t rcd = prv_cycles(part->t_rcd_ns, sd_hz);
  // A write must not end before the row may close, nor before the bank may open again
  uint32_t wr = PBL_MAX(PBL_MAX((uint32_t)part->t_wr_ck, ras - PBL_MIN(ras, rcd)),
                        rc - PBL_MIN(rc, rcd + rp));
  uint32_t xsr = prv_cycles(part->t_xsr_ns, sd_hz);
  uint32_t common;
  uint32_t sdtr;
  uint32_t sdcr;

  if (part->banks != 2U && part->banks != 4U) {
    return -EINVAL;
  }
  if (part->row_bits < 11U || part->row_bits > 13U || part->col_bits < 8U || part->col_bits > 11U ||
      part->cas_latency < 1U || part->cas_latency > 3U) {
    return -EINVAL;
  }
  if (part->width != 8U && part->width != 16U && part->width != 32U) {
    return -EINVAL;
  }

  if (part->t_mrd_ck > FMC_SDTR_MAX || xsr > FMC_SDTR_MAX || ras > FMC_SDTR_MAX ||
      wr > FMC_SDTR_MAX || rcd > FMC_SDTR_MAX || rc > FMC_SDTR_MAX || rp > FMC_SDTR_MAX) {
    return -EINVAL;
  }

  sdtr = prv_timing(part->t_mrd_ck, FMC_SDTR_TMRD_MASK) | prv_timing(xsr, FMC_SDTR_TXSR_MASK) |
         prv_timing(ras, FMC_SDTR_TRAS_MASK) | prv_timing(wr, FMC_SDTR_TWR_MASK) |
         prv_timing(rcd, FMC_SDTR_TRCD_MASK);
  // Both banks take the row cycle and the precharge time of bank 1
  common = prv_timing(rc, FMC_SDTR_TRC_MASK) | prv_timing(rp, FMC_SDTR_TRP_MASK);

  sdcr = PBL_FIELD_PREP(FMC_SDCR_NC_MASK, part->col_bits - 8U) |
         PBL_FIELD_PREP(FMC_SDCR_NR_MASK, part->row_bits - 11U) |
         PBL_FIELD_PREP(FMC_SDCR_MWID_MASK, part->width / 16U) |
         (part->banks == 4U ? FMC_SDCR_NB : 0U) |
         PBL_FIELD_PREP(FMC_SDCR_CAS_MASK, part->cas_latency);

  // The clock, read burst and read pipe are those of bank 1 for both
  pbl_writel(
      PBL_FIELD_PREP(FMC_SDCR_SDCLK_MASK, sdclk) | FMC_SDCR_RBURST | (bank == 0U ? sdcr : 0U),
      fmc->base + STM32H7_FMC_SDCR_OFFSET(0));
  pbl_writel(common | (bank == 0U ? sdtr : 0U), fmc->base + STM32H7_FMC_SDTR_OFFSET(0));
  if (bank != 0U) {
    pbl_writel(sdcr, fmc->base + STM32H7_FMC_SDCR_OFFSET(bank));
    pbl_writel(sdtr, fmc->base + STM32H7_FMC_SDTR_OFFSET(bank));
  }

  return 0;
}

static int prv_init(const struct pbl_ram *ram) {
  const struct pbl_ram_stm32_fmc *fmc = prv_fmc(ram);
  const struct pbl_ram_sdram_part *part = fmc->part;
  uint32_t region = (uint32_t)(fmc->addr >> 28);
  bool swap = region == 0x6U || region == 0x7U;
  uint32_t bank = (region == 0x7U || region == 0xdU) ? 1U : 0U;
  uint64_t rate_mhz;
  uint32_t ker_hz;
  uint32_t sd_hz;
  uint32_t sdclk;
  uint32_t ctb;
  int ret;

  if ((region != 0x6U && region != 0x7U && region != 0xcU && region != 0xdU) ||
      fmc->size !=
          ((size_t)part->banks << (part->row_bits + part->col_bits)) * (part->width / 8U)) {
    return -EINVAL;
  }

  ret = pbl_device_init(&fmc->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&fmc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_get_rate(&fmc->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&fmc->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  ker_hz = (uint32_t)(rate_mhz / 1000U);
  sdclk = ker_hz / 2U <= SDCLK_MAX_HZ ? 2U : 3U;
  sd_hz = ker_hz / sdclk;
  if (sd_hz > SDCLK_MAX_HZ) {
    return -EINVAL;
  }

  ret = prv_configure(fmc, bank, sd_hz, sdclk);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(FMC_BCR1_BMAP_MASK,
              PBL_FIELD_PREP(FMC_BCR1_BMAP_MASK, swap ? FMC_BCR1_BMAP_SWAP : 0U) | FMC_BCR1_FMCEN,
              fmc->base + STM32H7_FMC_BCR1_OFFSET);

  // The JEDEC power-up sequence
  ctb = bank == 0U ? FMC_SDCMR_CTB1 : FMC_SDCMR_CTB2;
  ret = prv_cmd(fmc, ctb, FMC_SDCMR_MODE_CLK_EN, 0U, 0U);
  if (ret != 0) {
    return ret;
  }
  pbl_busy_wait_us(POWER_UP_US);

  ret = prv_cmd(fmc, ctb, FMC_SDCMR_MODE_PALL, 0U, 0U);
  if (ret != 0) {
    return ret;
  }

  ret = prv_cmd(fmc, ctb, FMC_SDCMR_MODE_AUTO_REF, POWER_UP_REFRESHES - 1U, 0U);
  if (ret != 0) {
    return ret;
  }

  ret = prv_cmd(fmc, ctb, FMC_SDCMR_MODE_LOAD_MODE, 0U,
                ((uint32_t)part->cas_latency << MODE_CAS_POS) | MODE_SINGLE_WRITE);
  if (ret != 0) {
    return ret;
  }

  // Every row in the refresh period
  pbl_writel(
      PBL_FIELD_PREP(FMC_SDRTR_COUNT_MASK,
                     (uint32_t)((uint64_t)part->refresh_ms * sd_hz / 1000U >> part->row_bits) -
                         REFRESH_MARGIN),
      fmc->base + STM32H7_FMC_SDRTR_OFFSET);

  ram->state->base = fmc->addr;
  ram->state->size = fmc->size;

  return 0;
}

const struct pbl_ram_ops pbl_ram_stm32_fmc_ops = {
  .init = prv_init,
};

#include PBL_DT_INSTANCES(st_stm32h7_fmc)
