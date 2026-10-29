/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/audio/sf32lb52.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/reset/sf32lb.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/mcu/cache.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_pdm.h>

#define REG(name) (SF32LB52_PDM1_BASE + SF32LB52_PDM_##name##_OFFSET)

// Gain register: 0.5 dB steps from -15 dB
#define GAIN_MIN_MDB     (-15000)
#define GAIN_MAX_MDB     45000
#define GAIN_STEP_MDB    500
#define GAIN_DEFAULT_MDB 30000

#define HPF_COEFF        7U
#define PGA_GAIN_INIT    0x26U
#define BYTE_TRUNC_16BIT 1U

// Rates from the 9.6 MHz clock: clock / clk_div / sinc_rate / (2 or 4 with lpf_ds)
struct prv_rate {
  uint32_t rate;
  uint8_t clk_div;
  uint8_t sinc_rate;
  bool sinc_order4;
  bool lpf_ds;
};

static const struct prv_rate s_rates[] = {
  {8000U, 4U, 75U, false, true},
  {16000U, 6U, 50U, false, false},
  {32000U, 6U, 25U, false, false},
  {48000U, 4U, 25U, true, false},
};

static const struct pbl_clk s_clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_PDM1);
static const struct pbl_reset s_reset = PBL_RESET(SF32LB_HPSYS_RCC_RESET, SF32LB52_HPSYS_RST_PDM1);

static const struct pbl_audio_sf32lb52_pdm *prv_dev(const struct pbl_audio *audio) {
  return container_of(audio, const struct pbl_audio_sf32lb52_pdm, audio);
}

static const struct prv_rate *prv_find_rate(uint32_t rate) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_rates); i++) {
    if (s_rates[i].rate == rate) {
      return &s_rates[i];
    }
  }

  return nullptr;
}

static uint32_t prv_gain_reg(int32_t gain_mdb) {
  return (uint32_t)pbl_audio_gain_to_step(gain_mdb, GAIN_MIN_MDB, GAIN_STEP_MDB);
}

static void prv_configure(const struct prv_rate *rate, uint8_t channels) {
  uint32_t cfg0 = pbl_readl(REG(CFG0));
  uint32_t fifo = pbl_readl(REG(FIFO_CFG));
  uint32_t sinc = pbl_readl(REG(SINC_CFG));
  uint32_t lpf = pbl_readl(REG(LPF_CFG6));

  cfg0 &= ~(PDM_CFG0_LEFT_EN | PDM_CFG0_RIGHT_EN | PDM_CFG0_STEREO_EN | PDM_CFG0_SWAP_EN |
            PDM_CFG0_CLK_SEL | PDM_CFG0_CLK_DIV_MASK);
  fifo &= ~(PDM_FIFO_CFG_BYTE_CON | PDM_FIFO_CFG_RX_DMA_MSK_L | PDM_FIFO_CFG_RX_DMA_MSK_R |
            PDM_FIFO_CFG_PDM_SHIFT_MASK | PDM_FIFO_CFG_BYTE_TRUNC_MASK);
  sinc &= ~(PDM_SINC_CFG_SINC_ORDER_SEL | PDM_SINC_CFG_SINC_RATE_MASK);
  lpf &= ~PDM_LPF_CFG6_LPF_DS;

  if (channels == 1U) {
    cfg0 |= PDM_CFG0_LEFT_EN;
    fifo |= PDM_FIFO_CFG_RX_DMA_MSK_R;
  } else {
    cfg0 |= PDM_CFG0_LEFT_EN | PDM_CFG0_RIGHT_EN | PDM_CFG0_STEREO_EN;
    fifo |= PDM_FIFO_CFG_BYTE_CON;
  }

  cfg0 |= PBL_FIELD_PREP(PDM_CFG0_CLK_DIV_MASK, rate->clk_div);
  sinc |= PBL_FIELD_PREP(PDM_SINC_CFG_SINC_RATE_MASK, rate->sinc_rate) |
          (rate->sinc_order4 ? PDM_SINC_CFG_SINC_ORDER_SEL : 0U);
  lpf |= rate->lpf_ds ? PDM_LPF_CFG6_LPF_DS : 0U;
  fifo |= PBL_FIELD_PREP(PDM_FIFO_CFG_BYTE_TRUNC_MASK, BYTE_TRUNC_16BIT);

  pbl_writel(sinc, REG(SINC_CFG));
  pbl_writel(lpf, REG(LPF_CFG6));
  pbl_writel(cfg0, REG(CFG0));
  pbl_writel(fifo, REG(FIFO_CFG));
}

static void prv_set_gain_reg(int32_t gain_mdb) {
  uint32_t reg = prv_gain_reg(gain_mdb);

  pbl_writel(
      (pbl_readl(REG(PGA_CFG)) & ~(PDM_PGA_CFG_PGA_GAIN_L_MASK | PDM_PGA_CFG_PGA_GAIN_R_MASK)) |
          PBL_FIELD_PREP(PDM_PGA_CFG_PGA_GAIN_L_MASK, reg) |
          PBL_FIELD_PREP(PDM_PGA_CFG_PGA_GAIN_R_MASK, reg),
      REG(PGA_CFG));
}

static void prv_dma_done(void *arg, int status) {
  const struct pbl_audio *audio = arg;

  if (status != 0) {
    pbl_audio_report_xrun(audio);
    return;
  }

  pbl_audio_period_elapsed(audio);
}

void pbl_audio_sf32lb52_pdm_irq_handler(const struct pbl_audio_sf32lb52_pdm *pdm) {
  uint32_t st = pbl_readl(REG(INT_ST));

  pbl_writel(st & (PDM_INT_CLR_L | PDM_INT_CLR_R), REG(INT_CLR));
  if ((st & (PDM_INT_ST_OVERFLOW_L | PDM_INT_ST_OVERFLOW_R)) != 0U) {
    pbl_audio_report_xrun(&pdm->audio);
  }
}

static int prv_init(const struct pbl_audio *audio) {
  const struct pbl_audio_sf32lb52_pdm *pdm = prv_dev(audio);
  int ret;

  if (dcache_line_size() > PBL_AUDIO_BUF_ALIGN) {
    return -EINVAL;
  }

  ret = pbl_device_init(&s_clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&s_reset.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  if (pdm->vdd != nullptr) {
    ret = pbl_device_init(&pdm->vdd->dev);
    if (ret != 0) {
      return ret;
    }
  }

  return pbl_pinctrl_select_state(pdm->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
}

static void prv_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_rates); i++) {
    caps->rates |= pbl_audio_rate_bit(s_rates[i].rate);
  }
  caps->min_channels = 1U;
  caps->max_channels = 2U;
  caps->max_periods = 2U;
  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = GAIN_DEFAULT_MDB;
}

static int prv_hw_start(const struct pbl_audio_sf32lb52_pdm *pdm,
                        const struct pbl_audio_stream *stream, int32_t gain_mdb) {
  size_t period_bytes = stream->period_frames * stream->channels * sizeof(int16_t);
  const struct pbl_dma_slave_config cfg = {
    .src_addr = REG(DATA_L),
    .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_4_BYTES,
  };
  int ret;

  ret = pbl_reset_reset(&s_reset);
  if (ret != 0) {
    return ret;
  }

  pbl_writel(pbl_readl(REG(CFG0)) & ~PDM_CFG0_PDMCOREEN, REG(CFG0));
  prv_configure(prv_find_rate(stream->rate), stream->channels);
  pbl_writel((pbl_readl(REG(HPF_CFG)) & ~(PDM_HPF_CFG_HPF_BYPASS | PDM_HPF_CFG_HPF_COEFF_MASK)) |
                 PBL_FIELD_PREP(PDM_HPF_CFG_HPF_COEFF_MASK, HPF_COEFF),
             REG(HPF_CFG));
  pbl_writel(PBL_FIELD_PREP(PDM_PGA_CFG_PGA_GAIN_L_MASK, PGA_GAIN_INIT) |
                 PBL_FIELD_PREP(PDM_PGA_CFG_PGA_GAIN_R_MASK, PGA_GAIN_INIT),
             REG(PGA_CFG));
  pbl_writel(pbl_readl(REG(LPF_CFG6)) & ~PDM_LPF_CFG6_LPF_BYPASS, REG(LPF_CFG6));
  prv_set_gain_reg(gain_mdb);

  pdm->state->ring_bytes = period_bytes * stream->periods;
  dcache_flush_invalidate(stream->buf, pdm->state->ring_bytes);

  ret = pbl_dma_slave_config(pdm->dma, &cfg);
  if (ret != 0) {
    return ret;
  }

  pbl_writel(pbl_readl(REG(INT_MSK)) | PDM_INT_MSK_L | PDM_INT_MSK_R, REG(INT_MSK));
  ret = pbl_dma_prep_dma_cyclic(pdm->dma, stream->buf, pdm->state->ring_bytes, period_bytes,
                                PBL_DMA_DEV_TO_MEM, prv_dma_done, (void *)&pdm->audio);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(pdm->dma_irq);
  pbl_irq_enable(pdm->irq);
  pbl_writel(pbl_readl(REG(INT_MSK)) & ~(PDM_INT_MSK_L | PDM_INT_MSK_R), REG(INT_MSK));
  pbl_writel(pbl_readl(REG(CFG0)) | PDM_CFG0_PDMCOREEN, REG(CFG0));

  return 0;
}

static void prv_hw_stop(const struct pbl_audio_sf32lb52_pdm *pdm) {
  pbl_irq_disable(pdm->dma_irq);
  pbl_irq_disable(pdm->irq);
  (void)pbl_dma_terminate(pdm->dma);
  pbl_writel(pbl_readl(REG(CFG0)) & ~PDM_CFG0_PDMCOREEN, REG(CFG0));
}

static int prv_powered_start(const struct pbl_audio_sf32lb52_pdm *pdm,
                             const struct pbl_audio_stream *stream, int32_t gain_mdb) {
  int ret;

  ret = pbl_clk_enable(&s_clk);
  if (ret != 0) {
    return ret;
  }

  ret = prv_hw_start(pdm, stream, gain_mdb);
  if (ret != 0) {
    prv_hw_stop(pdm);
    (void)pbl_clk_disable(&s_clk);
  }

  return ret;
}

static int prv_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_sf32lb52_pdm *pdm = prv_dev(audio);
  int ret;

  if ((stream->period_frames * stream->channels * sizeof(int16_t)) % sizeof(uint32_t) != 0U) {
    return -EINVAL;
  }

  if (pdm->vdd != nullptr) {
    ret = pbl_regulator_enable(pdm->vdd);
    if (ret != 0) {
      return ret;
    }
  }

  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPWFI);

  ret = prv_powered_start(pdm, stream, mute ? GAIN_MIN_MDB : gain_mdb);
  if (ret != 0) {
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
    if (pdm->vdd != nullptr) {
      (void)pbl_regulator_disable(pdm->vdd);
    }
  }

  return ret;
}

static int prv_stop(const struct pbl_audio *audio) {
  const struct pbl_audio_sf32lb52_pdm *pdm = prv_dev(audio);
  int ret;

  prv_hw_stop(pdm);
  ret = pbl_clk_disable(&s_clk);

  soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);

  if (pdm->vdd != nullptr) {
    int err = pbl_regulator_disable(pdm->vdd);

    if (ret == 0) {
      ret = err;
    }
  }

  return ret;
}

static int prv_set_gain(const struct pbl_audio *audio, int32_t gain_mdb) {
  prv_set_gain_reg(gain_mdb);

  return 0;
}

static void prv_sync_for_cpu(const struct pbl_audio *audio, void *buf, size_t len) {
  dcache_invalidate(buf, len);
}

static int prv_pointer(const struct pbl_audio *audio, size_t *frames) {
  const struct pbl_audio_sf32lb52_pdm *pdm = prv_dev(audio);
  const struct pbl_audio_state *state = audio->state;
  size_t residue;
  int ret;

  ret = pbl_dma_tx_status(pdm->dma, &residue);
  if (ret != 0) {
    return ret;
  }

  *frames = ((pdm->state->ring_bytes - residue) % state->period_bytes) /
            (state->stream.channels * sizeof(int16_t));

  return 0;
}

const struct pbl_audio_ops pbl_audio_sf32lb52_pdm_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .set_gain = prv_set_gain,
  .sync_for_cpu = prv_sync_for_cpu,
  .pointer = prv_pointer,
};
