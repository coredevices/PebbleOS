/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/sdhc.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/sem.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_sdmmc.h>

// The FIFO holds one block: the CPU moves one at most in a command, the DMA as many as the data
// length counts, from buffers aligned for it
#define MAX_BLOCK_SIZE 512U
#define MAX_BLOCKS_PIO 1U
#define MAX_BLOCKS_DMA ((SDMMC_DLR_DATA_LEN_MASK + 1U) / MAX_BLOCK_SIZE)
#define DMA_ALIGN      PBL_MAX(PBL_DCACHE_LINE_SIZE, sizeof(uint32_t))

// The card takes 74 clock cycles after power-up, a millisecond at the identification clock
#define POWER_UP_US 1000U

#define MODE_TIMEOUT_US 10000U
#define CMD_TIMEOUT_US  100000U
#define BUSY_TIMEOUT_US 1000000U
#define DATA_TIMEOUT_US 1000000U
#define DATA_TIMEOUT_MS 1000U
#define DMA_DRAIN_US    1000U

// Bus clock cycles the data path waits for the card
#define DATA_TIMEOUT_CYCLES 0x02000000U

struct pbl_sdhc_sf32lb_sdmmc_state {
  struct pbl_sem done;
  uint8_t bus_width;
};

struct pbl_sdhc_sf32lb_sdmmc {
  struct pbl_sdhc sdhc;
  uintptr_t base;
  pbl_irq_t irqn;
  struct pbl_clk clk;
  struct pbl_pinctrl pinctrl;
  struct pbl_gpio cd;
  const struct pbl_dma_chan *dma;
  struct pbl_sdhc_sf32lb_sdmmc_state *state;
};

static const struct pbl_sdhc_sf32lb_sdmmc *prv_sdmmc(const struct pbl_sdhc *sdhc) {
  return PBL_CONTAINER_OF(sdhc, const struct pbl_sdhc_sf32lb_sdmmc, sdhc);
}

static inline uint32_t prv_getreg(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc, uint32_t offset) {
  return pbl_readl(sdmmc->base + offset);
}

static inline void prv_putreg(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, sdmmc->base + offset);
}

// The bus clock is the controller clock divided by DIV + 1, at most the rate asked for
static int prv_set_ios(const struct pbl_sdhc *sdhc, const struct pbl_sdhc_ios *ios) {
  const struct pbl_sdhc_sf32lb_sdmmc *sdmmc = prv_sdmmc(sdhc);
  uint64_t rate_mhz;
  uint32_t ker_hz;
  uint32_t div;
  int ret;

  if (ios->bus_width != 1U && ios->bus_width != 4U) {
    return -EINVAL;
  }

  ret = pbl_clk_get_rate(&sdmmc->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  ker_hz = (uint32_t)(rate_mhz / 1000U);
  div = ios->clock_hz == 0U ? 0U : (ker_hz + ios->clock_hz - 1U) / ios->clock_hz;
  if (div > SDMMC_CLKCR_DIV_MAX + 1U) {
    return -EINVAL;
  }

  prv_putreg(sdmmc, SF32LB57_SDMMC_CLKCR_OFFSET,
             PBL_FIELD_PREP(SDMMC_CLKCR_DIV_MASK, div > 0U ? div - 1U : 0U) |
                 SDMMC_CLKCR_VOID_FIFO_ERROR | (ios->clock_hz == 0U ? SDMMC_CLKCR_STOP_CLK : 0U));
  sdmmc->state->bus_width = ios->bus_width;

  return 0;
}

static int prv_wait(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc, uint32_t mask, uint32_t timeout_us) {
  return PBL_POLL_BUSY_US((prv_getreg(sdmmc, SF32LB57_SDMMC_SR_OFFSET) & mask) != 0U, 1U,
                          timeout_us);
}

// The data path; a read starts before its command, a write after it
static void prv_data_start(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc,
                           const struct pbl_sdhc_data *data) {
  prv_putreg(sdmmc, SF32LB57_SDMMC_DLR_OFFSET, data->block_size * data->blocks - 1U);
  prv_putreg(sdmmc, SF32LB57_SDMMC_DCR_OFFSET,
             PBL_FIELD_PREP(SDMMC_DCR_BLOCK_SIZE_MASK, data->block_size - 1U) |
                 PBL_FIELD_PREP(SDMMC_DCR_WIRE_MODE_MASK,
                                sdmmc->state->bus_width == 4U ? SDMMC_DCR_WIRE_MODE_4 : 0U) |
                 (data->write ? 0U : SDMMC_DCR_R_WN) | SDMMC_DCR_TRAN_DATA_EN |
                 SDMMC_DCR_DATA_START);
}

static int prv_data_status(uint32_t sr) {
  if ((sr & SDMMC_SR_DATA_TIMEOUT) != 0U) {
    return -ETIMEDOUT;
  }
  if ((sr & SDMMC_SR_DATA_CRC) != 0U) {
    return -EILSEQ;
  }

  return 0;
}

// The DMA takes word-aligned buffers; a read also takes whole cache lines, as it invalidates them
static bool prv_dma_capable(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc,
                            const struct pbl_sdhc_data *data) {
  uintptr_t addr = (uintptr_t)data->buf;
  size_t len = (size_t)data->block_size * data->blocks;

  if (sdmmc->dma == nullptr || addr % sizeof(uint32_t) != 0U) {
    return false;
  }

  return data->write || (addr % PBL_DCACHE_LINE_SIZE == 0U && len % PBL_DCACHE_LINE_SIZE == 0U);
}

// The DMA follows the FIFO's requests from before the command, the interrupt waking the thread
// once the data path ends
static int prv_dma_start(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc,
                         const struct pbl_sdhc_data *data) {
  size_t len = (size_t)data->block_size * data->blocks;
  int ret;

  if (data->write) {
    pbl_dcache_flush_range(data->buf, len);
  } else {
    pbl_dcache_invalidate_range(data->buf, len);
  }

  ret = pbl_dma_prep_slave_single(sdmmc->dma, data->buf, len,
                                  data->write ? PBL_DMA_MEM_TO_DEV : PBL_DMA_DEV_TO_MEM, nullptr,
                                  nullptr);
  if (ret != 0) {
    return ret;
  }

  pbl_sem_reset(&sdmmc->state->done);
  prv_putreg(sdmmc, SF32LB57_SDMMC_IER_OFFSET,
             SDMMC_IER_DATA_DONE | SDMMC_IER_DATA_CRC | SDMMC_IER_DATA_TIMEOUT);

  return 0;
}

// A read is in memory once the DMA has drained the FIFO too
static int prv_dma_wait(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc,
                        const struct pbl_sdhc_data *data) {
  size_t residue = 0U;
  uint32_t sr;
  int ret;

  if (data->write) {
    prv_data_start(sdmmc, data);
  }

  (void)pbl_sem_take(&sdmmc->state->done, PBL_MSEC(DATA_TIMEOUT_MS));
  sr = prv_getreg(sdmmc, SF32LB57_SDMMC_SR_OFFSET);
  if ((sr & (SDMMC_SR_DATA_DONE | SDMMC_SR_DATA_TIMEOUT)) == 0U) {
    return -ETIMEDOUT;
  }

  ret = prv_data_status(sr);
  if (ret != 0 || data->write) {
    return ret;
  }

  ret = PBL_POLL_BUSY_US(pbl_dma_tx_status(sdmmc->dma, &residue) == 0 && residue == 0U, 1U,
                         DMA_DRAIN_US);
  pbl_dcache_invalidate_range(data->buf, (size_t)data->block_size * data->blocks);

  return ret;
}

static int prv_pio(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc, const struct pbl_sdhc_data *data) {
  uintptr_t fifo = sdmmc->base + SF32LB57_SDMMC_FIFO_OFFSET;
  size_t words = data->block_size / sizeof(uint32_t);
  uint8_t *buf = data->buf;
  uint32_t word;
  uint32_t sr;
  int ret;

  if (data->write) {
    prv_data_start(sdmmc, data);
    for (size_t i = 0U; i < words; i++, buf += sizeof(word)) {
      memcpy(&word, buf, sizeof(word));
      pbl_writel(word, fifo);
    }
  }

  ret = prv_wait(sdmmc, SDMMC_SR_DATA_DONE | SDMMC_SR_DATA_TIMEOUT, DATA_TIMEOUT_US);
  sr = prv_getreg(sdmmc, SF32LB57_SDMMC_SR_OFFSET);
  if (ret != 0) {
    return ret;
  }
  ret = prv_data_status(sr);
  if (ret != 0) {
    return ret;
  }

  // A read waits whole in the FIFO
  for (size_t i = 0U; !data->write && i < words; i++, buf += sizeof(word)) {
    word = pbl_readl(fifo);
    memcpy(buf, &word, sizeof(word));
  }

  return 0;
}

// A long response comes last word first, without its CRC and end bit
static void prv_get_resp(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc, struct pbl_sdhc_cmd *cmd) {
  uint32_t rar[4];

  if ((cmd->flags & PBL_SDHC_RSP_136) == 0U) {
    cmd->resp[0] = prv_getreg(sdmmc, SF32LB57_SDMMC_RAR_OFFSET(0));
    return;
  }

  for (unsigned int i = 0U; i < 4U; i++) {
    rar[i] = prv_getreg(sdmmc, SF32LB57_SDMMC_RAR_OFFSET(3U - i));
  }
  cmd->resp[0] = (rar[0] << 8) | (rar[1] >> 24);
  cmd->resp[1] = (rar[1] << 8) | (rar[2] >> 24);
  cmd->resp[2] = (rar[2] << 8) | (rar[3] >> 24);
  cmd->resp[3] = rar[3] << 8;
}

static int prv_request(const struct pbl_sdhc *sdhc, struct pbl_sdhc_cmd *cmd,
                       const struct pbl_sdhc_data *data) {
  const struct pbl_sdhc_sf32lb_sdmmc *sdmmc = prv_sdmmc(sdhc);
  bool dma = data != nullptr && prv_dma_capable(sdmmc, data);
  uint32_t ccr;
  uint32_t sr;
  int ret;

  if (data != nullptr &&
      (data->blocks == 0U || data->blocks > (dma ? MAX_BLOCKS_DMA : MAX_BLOCKS_PIO) ||
       data->block_size > MAX_BLOCK_SIZE || data->block_size % sizeof(uint32_t) != 0U)) {
    return -EINVAL;
  }

  prv_putreg(sdmmc, SF32LB57_SDMMC_SR_OFFSET, SDMMC_SR_ALL);
  prv_putreg(sdmmc, SF32LB57_SDMMC_DCR_OFFSET, 0U);
  if (dma) {
    ret = prv_dma_start(sdmmc, data);
    if (ret != 0) {
      return ret;
    }
  }
  if (data != nullptr && !data->write) {
    prv_data_start(sdmmc, data);
  }

  ccr = PBL_FIELD_PREP(SDMMC_CCR_CMD_INDEX_MASK, cmd->opcode) | SDMMC_CCR_CMD_TX_EN |
        SDMMC_CCR_CMD_START;
  if ((cmd->flags & PBL_SDHC_RSP_PRESENT) != 0U) {
    ccr |= SDMMC_CCR_CMD_HAS_RSP;
  }
  if ((cmd->flags & PBL_SDHC_RSP_136) != 0U) {
    ccr |= SDMMC_CCR_CMD_LONG_RSP;
  }

  prv_putreg(sdmmc, SF32LB57_SDMMC_CAR_OFFSET, cmd->arg);
  prv_putreg(sdmmc, SF32LB57_SDMMC_CCR_OFFSET, ccr);

  ret = prv_wait(sdmmc, SDMMC_SR_CMD_DONE | SDMMC_SR_CMD_TIMEOUT, CMD_TIMEOUT_US);
  sr = prv_getreg(sdmmc, SF32LB57_SDMMC_SR_OFFSET);
  if (ret == 0 && (sr & SDMMC_SR_CMD_TIMEOUT) != 0U) {
    ret = -ETIMEDOUT;
  } else if (ret == 0 && (sr & SDMMC_SR_CMD_RSP_CRC) != 0U &&
             (cmd->flags & PBL_SDHC_RSP_CRC) != 0U) {
    ret = -EILSEQ;
  }

  if (ret == 0 && (cmd->flags & PBL_SDHC_RSP_PRESENT) != 0U) {
    prv_get_resp(sdmmc, cmd);
  }

  if (ret == 0 && (cmd->flags & PBL_SDHC_RSP_BUSY) != 0U) {
    ret = PBL_POLL_BUSY_US((prv_getreg(sdmmc, SF32LB57_SDMMC_DSR_OFFSET) & SDMMC_DSR_DAT0) != 0U,
                           10U, BUSY_TIMEOUT_US);
  }

  if (ret == 0 && data != nullptr) {
    ret = dma ? prv_dma_wait(sdmmc, data) : prv_pio(sdmmc, data);
  }

  if (dma) {
    prv_putreg(sdmmc, SF32LB57_SDMMC_IER_OFFSET, 0U);
    (void)pbl_dma_terminate(sdmmc->dma);
  }
  prv_putreg(sdmmc, SF32LB57_SDMMC_DCR_OFFSET, 0U);
  prv_putreg(sdmmc, SF32LB57_SDMMC_SR_OFFSET, SDMMC_SR_ALL);

  return ret;
}

static void prv_isr(const struct pbl_sdhc_sf32lb_sdmmc *sdmmc) {
  prv_putreg(sdmmc, SF32LB57_SDMMC_IER_OFFSET, 0U);
  pbl_sem_give(&sdmmc->state->done);
}

static int prv_get_cd(const struct pbl_sdhc *sdhc) {
  const struct pbl_sdhc_sf32lb_sdmmc *sdmmc = prv_sdmmc(sdhc);

  if (sdmmc->cd.port == nullptr) {
    return 1;
  }

  return pbl_gpio_get(&sdmmc->cd);
}

// The controller leaves reset in the mode that maps the card into memory: the registers drive it
// once it hands them over
static int prv_init(const struct pbl_sdhc *sdhc) {
  const struct pbl_sdhc_sf32lb_sdmmc *sdmmc = prv_sdmmc(sdhc);
  int ret;

  pbl_sem_init(&sdmmc->state->done, 0U, 1U);

  if (sdmmc->dma != nullptr) {
    const struct pbl_dma_slave_config cfg = {
      .src_addr = sdmmc->base + SF32LB57_SDMMC_FIFO_OFFSET,
      .dst_addr = sdmmc->base + SF32LB57_SDMMC_FIFO_OFFSET,
      .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_4_BYTES,
      .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_4_BYTES,
    };

    ret = pbl_device_init(&sdmmc->dma->ctrl->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_dma_slave_config(sdmmc->dma, &cfg);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_device_init(&sdmmc->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&sdmmc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&sdmmc->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  if (sdmmc->cd.port != nullptr) {
    ret = pbl_gpio_configure(&sdmmc->cd, PBL_GPIO_INPUT);
    if (ret != 0) {
      return ret;
    }
  }

  prv_putreg(sdmmc, SF32LB57_SDMMC_IER_OFFSET, 0U);
  prv_putreg(sdmmc, SF32LB57_SDMMC_CDR_OFFSET, 0U);
  prv_putreg(sdmmc, SF32LB57_SDMMC_TOR_OFFSET, DATA_TIMEOUT_CYCLES);
  ret = prv_set_ios(sdhc, &(struct pbl_sdhc_ios){.clock_hz = 400000U, .bus_width = 1U});
  if (ret != 0) {
    return ret;
  }
  pbl_sleep_us(POWER_UP_US);

  pbl_modifyl(0U, SDMMC_CASR_SD_REQ, sdmmc->base + SF32LB57_SDMMC_CASR_OFFSET);

  ret = PBL_POLL_BUSY_US((prv_getreg(sdmmc, SF32LB57_SDMMC_CASR_OFFSET) & SDMMC_CASR_SD_BUSY) != 0U,
                         1U, MODE_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(sdmmc->irqn);

  return 0;
}

static const struct pbl_sdhc_ops s_ops = {
  .init = prv_init,
  .set_ios = prv_set_ios,
  .request = prv_request,
  .get_cd = prv_get_cd,
};

#define PBL_SDHC_SF32LB_SDMMC_DEFINE(sym, _name, _base, _irq, _clk, _pinctrl, _bus_width, _max_hz, \
                                     _cd, _dma)                                                    \
  PBL_SDHC_STATE_DEFINE(sym);                                                                      \
  static struct pbl_sdhc_sf32lb_sdmmc_state sym##_sdmmc_state;                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_sdhc_sf32lb_sdmmc, struct pbl_sdhc, sdhc){                   \
    .sdhc = PBL_SDHC_INIT(sym, _name, nullptr, &s_ops, _bus_width, _max_hz,                        \
                          (_dma) != nullptr ? MAX_BLOCKS_DMA : MAX_BLOCKS_PIO, DMA_ALIGN),         \
    .base = (_base),                                                                               \
    .irqn = (_irq),                                                                                \
    .clk = _clk,                                                                                   \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                  \
    .cd = _cd,                                                                                     \
    .dma = (_dma),                                                                                 \
    .state = &sym##_sdmmc_state,                                                                   \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_SDHC_SF32LB_SDMMC_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(sifli_sf32lb57_sdmmc)
