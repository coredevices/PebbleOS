/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/sdhc.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/time.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_sdmmc.h>

// The SDMMC of the STM32H7, as the PL18x variant its primecell ID tells
#define PERIPHID_STM32H7 0x10153180U

// The card takes 74 clock cycles after power-up, a millisecond at the identification clock
#define POWER_UP_US 1000U

#define CMD_TIMEOUT_US  100000U
#define BUSY_TIMEOUT_US 1000000U
#define DATA_TIMEOUT_MS 1000U

#define CMD_STOP_TRANSMISSION 12U

#define CMD_ERRORS (SDMMC_STAR_CCRCFAIL | SDMMC_STAR_CTIMEOUT)
#define DATA_ERRORS \
  (SDMMC_STAR_DCRCFAIL | SDMMC_STAR_DTIMEOUT | SDMMC_STAR_TXUNDERR | SDMMC_STAR_RXOVERR)

struct pbl_sdhc_stm32_sdmmc_state {
  struct pbl_sem done;
};

struct pbl_sdhc_stm32_sdmmc {
  struct pbl_sdhc sdhc;
  uintptr_t base;
  pbl_irq_t irqn;
  struct pbl_clk clk;
  struct pbl_pinctrl pinctrl;
  struct pbl_gpio cd;
  uint32_t periphid;
  bool neg_edge;
  struct pbl_sdhc_stm32_sdmmc_state *state;
};

static const struct pbl_sdhc_stm32_sdmmc *prv_sdmmc(const struct pbl_sdhc *sdhc) {
  return PBL_CONTAINER_OF(sdhc, const struct pbl_sdhc_stm32_sdmmc, sdhc);
}

static inline uint32_t prv_getreg(const struct pbl_sdhc_stm32_sdmmc *sdmmc, uint32_t offset) {
  return pbl_readl(sdmmc->base + offset);
}

static inline void prv_putreg(const struct pbl_sdhc_stm32_sdmmc *sdmmc, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, sdmmc->base + offset);
}

// The bus clock is the kernel clock divided by twice CLKDIV, or the kernel clock itself, at most
// the rate asked for; the flow control stops it when the FIFO is full or empty, so the CPU never
// overruns it
static int prv_set_ios(const struct pbl_sdhc *sdhc, const struct pbl_sdhc_ios *ios) {
  const struct pbl_sdhc_stm32_sdmmc *sdmmc = prv_sdmmc(sdhc);
  uint64_t rate_mhz;
  uint32_t ker_hz;
  uint32_t clkdiv;
  int ret;

  if (ios->bus_width != 1U && ios->bus_width != 4U) {
    return -EINVAL;
  }

  ret = pbl_clk_get_rate(&sdmmc->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  ker_hz = (uint32_t)(rate_mhz / 1000U);
  if (ios->clock_hz == 0U) {
    clkdiv = SDMMC_CLKCR_CLKDIV_MASK;
  } else if (ker_hz <= ios->clock_hz) {
    clkdiv = 0U;
  } else {
    clkdiv = PBL_DIV_ROUND_UP(ker_hz, 2U * ios->clock_hz);
  }
  if (clkdiv > SDMMC_CLKCR_CLKDIV_MASK) {
    return -EINVAL;
  }

  prv_putreg(sdmmc, STM32H7_SDMMC_CLKCR_OFFSET,
             PBL_FIELD_PREP(SDMMC_CLKCR_CLKDIV_MASK, clkdiv) |
                 PBL_FIELD_PREP(SDMMC_CLKCR_WIDBUS_MASK,
                                ios->bus_width == 4U ? SDMMC_CLKCR_WIDBUS_4 : 0U) |
                 (sdmmc->neg_edge ? SDMMC_CLKCR_NEGEDGE : 0U) | SDMMC_CLKCR_HWFC_EN);

  return 0;
}

static int prv_wait_flags(const struct pbl_sdhc_stm32_sdmmc *sdmmc, uint32_t mask,
                          uint32_t timeout_us) {
  return PBL_POLL_BUSY_US((prv_getreg(sdmmc, STM32H7_SDMMC_STAR_OFFSET) & mask) != 0U, 1U,
                          timeout_us);
}

static uint32_t prv_waitresp(uint32_t flags) {
  if ((flags & PBL_SDHC_RSP_PRESENT) == 0U) {
    return 0U;
  }
  if ((flags & PBL_SDHC_RSP_136) != 0U) {
    return SDMMC_CMDR_WAITRESP_LONG;
  }

  return (flags & PBL_SDHC_RSP_CRC) != 0U ? SDMMC_CMDR_WAITRESP_SHORT
                                          : SDMMC_CMDR_WAITRESP_SHORT_NO_CRC;
}

static int prv_data_status(uint32_t star) {
  if ((star & SDMMC_STAR_DCRCFAIL) != 0U) {
    return -EILSEQ;
  }
  if ((star & SDMMC_STAR_DTIMEOUT) != 0U) {
    return -ETIMEDOUT;
  }
  if ((star & (DATA_ERRORS | SDMMC_STAR_IDMATE)) != 0U) {
    return -EIO;
  }

  return 0;
}

// The internal DMA takes word-aligned buffers; a read also takes whole cache lines, as it
// invalidates them
static bool prv_dma_capable(const struct pbl_sdhc_data *data) {
  uintptr_t addr = (uintptr_t)data->buf;
  size_t len = (size_t)data->block_size * data->blocks;

  if (addr % sizeof(uint32_t) != 0U) {
    return false;
  }

  return data->write || (addr % PBL_DCACHE_LINE_SIZE == 0U && len % PBL_DCACHE_LINE_SIZE == 0U);
}

static void prv_dma_start(const struct pbl_sdhc_stm32_sdmmc *sdmmc,
                          const struct pbl_sdhc_data *data) {
  size_t len = (size_t)data->block_size * data->blocks;

  if (data->write) {
    pbl_dcache_flush_range(data->buf, len);
  } else {
    pbl_dcache_invalidate_range(data->buf, len);
  }

  pbl_sem_reset(&sdmmc->state->done);
  prv_putreg(sdmmc, STM32H7_SDMMC_IDMABASE0R_OFFSET, (uint32_t)(uintptr_t)data->buf);
  prv_putreg(sdmmc, STM32H7_SDMMC_IDMACTRLR_OFFSET, SDMMC_IDMACTRLR_IDMAEN);
  prv_putreg(sdmmc, STM32H7_SDMMC_MASKR_OFFSET, DATA_ERRORS | SDMMC_STAR_DATAEND);
}

// Until the data path ends, the interrupt waking the thread
static int prv_dma_wait(const struct pbl_sdhc_stm32_sdmmc *sdmmc,
                        const struct pbl_sdhc_data *data) {
  uint32_t star;
  int ret;

  ret = pbl_sem_take(&sdmmc->state->done, PBL_MSEC(DATA_TIMEOUT_MS));
  star = prv_getreg(sdmmc, STM32H7_SDMMC_STAR_OFFSET);

  if (!data->write) {
    pbl_dcache_invalidate_range(data->buf, (size_t)data->block_size * data->blocks);
  }

  if (ret != 0 && (star & (DATA_ERRORS | SDMMC_STAR_DATAEND)) == 0U) {
    return -ETIMEDOUT;
  }

  return prv_data_status(star);
}

// Through the FIFO, half of it at a time while the transfer runs, then what is left
static int prv_pio(const struct pbl_sdhc_stm32_sdmmc *sdmmc, const struct pbl_sdhc_data *data) {
  const uint32_t ready = data->write ? SDMMC_STAR_TXFIFOHE : SDMMC_STAR_RXFIFOHF;
  uintptr_t fifo = sdmmc->base + STM32H7_SDMMC_FIFOR_OFFSET;
  size_t words = (size_t)data->block_size * data->blocks / sizeof(uint32_t);
  uint64_t deadline = pbl_uptime_ms() + DATA_TIMEOUT_MS;
  uint8_t *buf = data->buf;
  uint32_t star;
  uint32_t word;
  int ret;

  while (true) {
    star = prv_getreg(sdmmc, STM32H7_SDMMC_STAR_OFFSET);
    if ((star & DATA_ERRORS) != 0U) {
      break;
    }

    if (words > 0U && (star & ready) != 0U) {
      for (size_t n = PBL_MIN(words, (size_t)SDMMC_FIFO_HALF_WORDS); n > 0U; n--) {
        if (data->write) {
          memcpy(&word, buf, sizeof(word));
          pbl_writel(word, fifo);
        } else {
          word = pbl_readl(fifo);
          memcpy(buf, &word, sizeof(word));
        }
        buf += sizeof(word);
        words--;
      }
      continue;
    }

    if ((star & SDMMC_STAR_DATAEND) != 0U) {
      break;
    }

    if (pbl_uptime_ms() > deadline) {
      return -ETIMEDOUT;
    }
  }

  ret = prv_data_status(star);
  if (ret != 0) {
    return ret;
  }

  // The last words of a read wait in the FIFO
  while (!data->write && words > 0U &&
         (prv_getreg(sdmmc, STM32H7_SDMMC_STAR_OFFSET) & SDMMC_STAR_RXFIFOE) == 0U) {
    word = pbl_readl(fifo);
    memcpy(buf, &word, sizeof(word));
    buf += sizeof(word);
    words--;
  }

  return words == 0U ? 0 : -EIO;
}

static int prv_request(const struct pbl_sdhc *sdhc, struct pbl_sdhc_cmd *cmd,
                       const struct pbl_sdhc_data *data) {
  const struct pbl_sdhc_stm32_sdmmc *sdmmc = prv_sdmmc(sdhc);
  uint32_t waitresp = prv_waitresp(cmd->flags);
  bool dma = data != nullptr && prv_dma_capable(data);
  uint32_t cmdr;
  uint32_t star;
  int ret;

  prv_putreg(sdmmc, STM32H7_SDMMC_ICR_OFFSET, SDMMC_ICR_ALL);

  cmdr = PBL_FIELD_PREP(SDMMC_CMDR_CMDINDEX_MASK, cmd->opcode) |
         PBL_FIELD_PREP(SDMMC_CMDR_WAITRESP_MASK, waitresp) | SDMMC_CMDR_CPSMEN;
  if (cmd->opcode == CMD_STOP_TRANSMISSION) {
    cmdr |= SDMMC_CMDR_CMDSTOP;
  }

  // The command starts the data path, set up before it
  if (data != nullptr) {
    if (data->block_size == 0U || (data->block_size & (data->block_size - 1U)) != 0U ||
        data->block_size % sizeof(uint32_t) != 0U) {
      return -EINVAL;
    }
    prv_putreg(sdmmc, STM32H7_SDMMC_DTIMER_OFFSET, UINT32_MAX);
    prv_putreg(sdmmc, STM32H7_SDMMC_DLENR_OFFSET, data->block_size * data->blocks);
    prv_putreg(sdmmc, STM32H7_SDMMC_DCTRL_OFFSET,
               PBL_FIELD_PREP(SDMMC_DCTRL_DBLOCKSIZE_MASK, (uint32_t)PBL_CTZ(data->block_size)) |
                   (data->write ? 0U : SDMMC_DCTRL_DTDIR));
    if (dma) {
      prv_dma_start(sdmmc, data);
    }
    cmdr |= SDMMC_CMDR_CMDTRANS;
  }

  prv_putreg(sdmmc, STM32H7_SDMMC_ARGR_OFFSET, cmd->arg);
  prv_putreg(sdmmc, STM32H7_SDMMC_CMDR_OFFSET, cmdr);

  ret =
      prv_wait_flags(sdmmc, (waitresp != 0U ? SDMMC_STAR_CMDREND : SDMMC_STAR_CMDSENT) | CMD_ERRORS,
                     CMD_TIMEOUT_US);
  star = prv_getreg(sdmmc, STM32H7_SDMMC_STAR_OFFSET);
  if (ret == 0 && (star & SDMMC_STAR_CTIMEOUT) != 0U) {
    ret = -ETIMEDOUT;
  } else if (ret == 0 && (star & SDMMC_STAR_CCRCFAIL) != 0U &&
             (cmd->flags & PBL_SDHC_RSP_CRC) != 0U) {
    ret = -EILSEQ;
  }

  if (ret == 0 && waitresp != 0U) {
    for (unsigned int i = 0U; i < 4U; i++) {
      cmd->resp[i] = prv_getreg(sdmmc, STM32H7_SDMMC_RESPR_OFFSET(i));
    }
  }

  if (ret == 0 && (cmd->flags & PBL_SDHC_RSP_BUSY) != 0U &&
      (prv_getreg(sdmmc, STM32H7_SDMMC_STAR_OFFSET) & SDMMC_STAR_BUSYD0) != 0U) {
    ret = prv_wait_flags(sdmmc, SDMMC_STAR_BUSYD0END, BUSY_TIMEOUT_US);
  }

  if (ret == 0 && data != nullptr) {
    ret = dma ? prv_dma_wait(sdmmc, data) : prv_pio(sdmmc, data);
  }

  prv_putreg(sdmmc, STM32H7_SDMMC_MASKR_OFFSET, 0U);
  prv_putreg(sdmmc, STM32H7_SDMMC_IDMACTRLR_OFFSET, 0U);
  prv_putreg(sdmmc, STM32H7_SDMMC_DCTRL_OFFSET, 0U);
  prv_putreg(sdmmc, STM32H7_SDMMC_ICR_OFFSET, SDMMC_ICR_ALL);

  return ret;
}

static void prv_isr(const struct pbl_sdhc_stm32_sdmmc *sdmmc) {
  prv_putreg(sdmmc, STM32H7_SDMMC_MASKR_OFFSET, 0U);
  pbl_sem_give(&sdmmc->state->done);
}

static int prv_get_cd(const struct pbl_sdhc *sdhc) {
  const struct pbl_sdhc_stm32_sdmmc *sdmmc = prv_sdmmc(sdhc);

  if (sdmmc->cd.port == nullptr) {
    return 1;
  }

  return pbl_gpio_get(&sdmmc->cd);
}

static int prv_init(const struct pbl_sdhc *sdhc) {
  const struct pbl_sdhc_stm32_sdmmc *sdmmc = prv_sdmmc(sdhc);
  int ret;

  if (sdmmc->periphid != PERIPHID_STM32H7) {
    return -ENODEV;
  }

  pbl_sem_init(&sdmmc->state->done, 0U, 1U);

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

  prv_putreg(sdmmc, STM32H7_SDMMC_MASKR_OFFSET, 0U);
  ret = prv_set_ios(sdhc, &(struct pbl_sdhc_ios){.clock_hz = 400000U, .bus_width = 1U});
  if (ret != 0) {
    return ret;
  }
  prv_putreg(sdmmc, STM32H7_SDMMC_POWER_OFFSET, SDMMC_POWER_PWRCTRL_ON);
  pbl_sleep_us(POWER_UP_US);

  pbl_irq_enable(sdmmc->irqn);

  return 0;
}

static const struct pbl_sdhc_ops s_ops = {
  .init = prv_init,
  .set_ios = prv_set_ios,
  .request = prv_request,
  .get_cd = prv_get_cd,
};

#define PBL_SDHC_STM32_SDMMC_DEFINE(sym, _name, _base, _irq, _clk, _pinctrl, _bus_width, _max_hz, \
                                    _periphid, _neg_edge, _cd)                                    \
  PBL_SDHC_STATE_DEFINE(sym);                                                                     \
  static struct pbl_sdhc_stm32_sdmmc_state sym##_sdmmc_state;                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_sdhc_stm32_sdmmc, struct pbl_sdhc, sdhc){                   \
    .sdhc = PBL_SDHC_INIT(sym, _name, nullptr, &s_ops, _bus_width, _max_hz, 0U, 0U),              \
    .base = (_base),                                                                              \
    .irqn = (_irq),                                                                               \
    .clk = _clk,                                                                                  \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                 \
    .cd = _cd,                                                                                    \
    .periphid = (_periphid),                                                                      \
    .neg_edge = (_neg_edge),                                                                      \
    .state = &sym##_sdmmc_state,                                                                  \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_SDHC_STM32_SDMMC_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(arm_pl18x)
