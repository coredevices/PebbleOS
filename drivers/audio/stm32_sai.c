/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/stm32_sai.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/st/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <hardware/stm32_sai.h>

static_assert(PBL_DCACHE_LINE_SIZE <= PBL_AUDIO_BUF_ALIGN,
              "ring buffers must own their cache lines");

// The master clock the codec takes, in multiples of the rate
#define MCLK_FS 256U

// I2S frame: two 32-bit slots, the frame sync low for the left one
#define FRAME_BITS 64U
#define SLOT_BITS  32U

#define MCKDIV_MAX 63U

// The sub-block disables itself at the end of the frame, a few microseconds at the lowest rate
#define DISABLE_TIMEOUT_US 1000U

static const uint32_t s_rates[] = {8000U, 16000U, 32000U, 48000U};

static const struct pbl_audio_stm32_sai *prv_dev(const struct pbl_audio_dai *dai) {
  return PBL_CONTAINER_OF(dai, const struct pbl_audio_stm32_sai, dai);
}

static inline uint32_t prv_getreg(const struct pbl_audio_stm32_sai *sai, uint32_t offset) {
  return pbl_readl(sai->base + offset);
}

static inline void prv_putreg(const struct pbl_audio_stm32_sai *sai, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, sai->base + offset);
}

static void prv_dma_done(void *arg, int status) {
  const struct pbl_audio_dai *dai = arg;

  if (status != 0) {
    pbl_audio_report_xrun(pbl_audio_dai_card(dai));
    return;
  }

  pbl_audio_period_elapsed(pbl_audio_dai_card(dai));
}

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_stm32_sai *sai = prv_dev(dai);
  int ret;

  ret = pbl_device_init(&sai->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&sai->dma->ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_pinctrl_select_state(sai->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
}

static void prv_get_caps(const struct pbl_audio_dai *dai, struct pbl_audio_caps *caps) {
  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_rates); i++) {
    caps->rates |= pbl_audio_rate_bit(s_rates[i]);
  }
  caps->min_channels = 1U;
  caps->max_channels = 2U;
  caps->max_periods = 2U;
}

// The master clock is the kernel clock divided to the nearest of 256 times the rate
static int prv_mckdiv(const struct pbl_audio_stm32_sai *sai, uint32_t rate, uint32_t *mckdiv) {
  uint64_t rate_mhz;
  uint64_t div;
  int ret;

  ret = pbl_clk_get_rate(&sai->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  div = (rate_mhz + (uint64_t)rate * MCLK_FS * PBL_MILLIHZ_PER_HZ / 2U) /
        ((uint64_t)rate * MCLK_FS * PBL_MILLIHZ_PER_HZ);
  if (div == 0U || div > MCKDIV_MAX) {
    return -EINVAL;
  }

  *mckdiv = (uint32_t)div;

  return 0;
}

static void prv_disable(const struct pbl_audio_stm32_sai *sai) {
  pbl_modifyl(SAI_CR1_DMAEN | SAI_CR1_SAIEN, 0U, sai->base + STM32_SAI_CR1_OFFSET);
  (void)PBL_POLL_BUSY_US((prv_getreg(sai, STM32_SAI_CR1_OFFSET) & SAI_CR1_SAIEN) == 0U, 1U,
                         DISABLE_TIMEOUT_US);
  prv_putreg(sai, STM32_SAI_CR2_OFFSET, SAI_CR2_FFLUSH);
  prv_putreg(sai, STM32_SAI_CLRFR_OFFSET, SAI_CLRFR_ALL);
}

static int prv_hw_start(const struct pbl_audio_stm32_sai *sai,
                        const struct pbl_audio_stream *stream) {
  size_t period_bytes = stream->period_frames * stream->channels * sizeof(int16_t);
  const struct pbl_dma_slave_config cfg = {
    .dst_addr = sai->base + STM32_SAI_DR_OFFSET,
    .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_2_BYTES,
  };
  uint32_t mckdiv;
  int ret;

  ret = prv_mckdiv(sai, stream->rate, &mckdiv);
  if (ret != 0) {
    return ret;
  }

  prv_disable(sai);

  // Data changes on the falling edge of the bit clock, for the codec to sample on the rising one
  prv_putreg(sai, STM32_SAI_CR1_OFFSET,
             PBL_FIELD_PREP(SAI_CR1_MODE_MASK, SAI_CR1_MODE_MASTER_TX) |
                 PBL_FIELD_PREP(SAI_CR1_DS_MASK, SAI_CR1_DS_16) | SAI_CR1_CKSTR | SAI_CR1_OUTDRIV |
                 (stream->channels == 1U ? SAI_CR1_MONO : 0U) |
                 PBL_FIELD_PREP(SAI_CR1_MCKDIV_MASK, mckdiv) | SAI_CR1_MCKEN);
  prv_putreg(sai, STM32_SAI_CR2_OFFSET, PBL_FIELD_PREP(SAI_CR2_FTH_MASK, SAI_CR2_FTH_QUARTER));
  prv_putreg(sai, STM32_SAI_FRCR_OFFSET,
             PBL_FIELD_PREP(SAI_FRCR_FRL_MASK, FRAME_BITS - 1U) |
                 PBL_FIELD_PREP(SAI_FRCR_FSALL_MASK, FRAME_BITS / 2U - 1U) | SAI_FRCR_FSDEF |
                 SAI_FRCR_FSOFF);
  prv_putreg(
      sai, STM32_SAI_SLOTR_OFFSET,
      PBL_FIELD_PREP(SAI_SLOTR_SLOTSZ_MASK, SAI_SLOTR_SLOTSZ_32) |
          PBL_FIELD_PREP(SAI_SLOTR_NBSLOT_MASK, FRAME_BITS / SLOT_BITS - 1U) |
          PBL_FIELD_PREP(SAI_SLOTR_SLOTEN_MASK, PBL_GENMASK(FRAME_BITS / SLOT_BITS - 1U, 0)));

  pbl_dcache_flush_range(stream->buf, period_bytes * stream->periods);

  ret = pbl_dma_slave_config(sai->dma, &cfg);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_dma_prep_dma_cyclic(sai->dma, stream->buf, period_bytes * stream->periods, period_bytes,
                                PBL_DMA_MEM_TO_DEV, prv_dma_done, (void *)&sai->dai);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(0U, SAI_CR1_DMAEN, sai->base + STM32_SAI_CR1_OFFSET);
  pbl_modifyl(0U, SAI_CR1_SAIEN, sai->base + STM32_SAI_CR1_OFFSET);

  return 0;
}

// The kernel clock stops in Stop
static int prv_start(const struct pbl_audio_dai *dai, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_stm32_sai *sai = prv_dev(dai);
  int ret;

  ret = pbl_clk_enable(&sai->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_idle_state_block(SOC_STM32_IDLE_STOP);
  ret = prv_hw_start(sai, stream);
  if (ret != 0) {
    prv_disable(sai);
    (void)pbl_dma_terminate(sai->dma);
    pbl_idle_state_release(SOC_STM32_IDLE_STOP);
    (void)pbl_clk_disable(&sai->clk);
  }

  return ret;
}

static int prv_stop(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_stm32_sai *sai = prv_dev(dai);

  prv_disable(sai);
  (void)pbl_dma_terminate(sai->dma);
  pbl_idle_state_release(SOC_STM32_IDLE_STOP);

  return pbl_clk_disable(&sai->clk);
}

static void prv_sync_for_device(const struct pbl_audio_dai *dai, void *buf, size_t len) {
  pbl_dcache_flush_range(buf, len);
}

const struct pbl_audio_dai_ops pbl_audio_stm32_sai_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .sync_for_device = prv_sync_for_device,
};

#include PBL_DT_INSTANCES(stm32_sai)
