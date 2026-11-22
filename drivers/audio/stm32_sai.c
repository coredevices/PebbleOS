/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/stm32_sai.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/st/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/stm32_sai.h>

static_assert(PBL_DCACHE_LINE_SIZE <= PBL_AUDIO_BUF_ALIGN,
              "ring buffers must own their cache lines");

// The master clock the codec takes, in multiples of the rate
#define MCLK_FS 256U

// I2S frame: two 32-bit slots, the frame sync low for the left one
#define FRAME_BITS 64U
#define SLOT_BITS  32U

// PDM frame: a byte of each microphone of the pair, over 8 clocks
#define PDM_FRAME_BITS 16U

// PDM clock rate, in multiples of the rate, and the lowest the microphones run at
#define PDM_DECIMATION    64U
#define PDM_CK_MIN_HZ     1000000U
#define PDM_GAIN_DEFAULT  24000
#define PDM_GAIN_STEP_MDB 1000

// Raw samples the DMA controller hands over at a time
#define PDM_HALF_MS 2U

// Time microphones take to settle once clocked, silenced
#define PDM_SETTLE_MS 100U

// Byte of each microphone in a sample of the pair
#define PDM_LEFT_BYTE 1U

#define MCKDIV_MAX 63U

// The sub-block disables itself at the end of the frame, a few microseconds at the lowest rate
#define DISABLE_TIMEOUT_US 1000U

// Clock rates are in mHz
#define MHZ_PER_HZ 1000ULL

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

static bool prv_capture(const struct pbl_audio_stm32_sai *sai) {
  return sai->dai.dir == PBL_AUDIO_CAPTURE;
}

static void prv_dma_done(void *arg, int status) {
  const struct pbl_audio_dai *dai = arg;

  if (status != 0) {
    pbl_audio_report_xrun(pbl_audio_dai_card(dai));
    return;
  }

  pbl_audio_period_elapsed(pbl_audio_dai_card(dai));
}

// The raw samples of a half of the ring go through the filters into the periods of the stream
static void prv_pdm_done(void *arg, int status) {
  const struct pbl_audio_stm32_sai *sai = arg;
  const struct pbl_audio *card = pbl_audio_dai_card(&sai->dai);
  struct pbl_audio_stm32_sai_state *st = sai->state;
  const struct pbl_audio_stream *stream = &st->stream;
  const size_t bytes = st->decimation / 8U;
  const uint8_t *in;
  size_t frames;

  if (status != 0) {
    pbl_audio_report_xrun(card);
    return;
  }

  in = st->ring + st->half * st->half_bytes;
  st->half ^= 1U;
  pbl_dcache_invalidate_range((void *)in, st->half_bytes);

  frames = st->half_bytes / (2U * bytes);
  while (frames > 0U) {
    size_t n = PBL_MIN(frames, stream->period_frames - st->frame);
    int16_t *out = (int16_t *)stream->buf +
                   ((size_t)st->period * stream->period_frames + st->frame) * stream->channels;

    for (uint8_t ch = 0U; ch < stream->channels; ch++) {
      (void)pbl_pdm_filter_run(&st->filters[ch],
                               in + (ch == 0U ? PDM_LEFT_BYTE : 1U - PDM_LEFT_BYTE), n * bytes, 2U,
                               out + ch, stream->channels);
    }
    if (st->mute) {
      memset(out, 0, n * stream->channels * sizeof(int16_t));
    }

    in += n * bytes * 2U;
    frames -= n;
    st->frame += n;
    if (st->frame == stream->period_frames) {
      st->frame = 0U;
      st->period = (st->period + 1U) % stream->periods;
      pbl_audio_period_elapsed(card);
    }
  }
}

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_stm32_sai *sai = prv_dev(dai);
  int ret;

  // Capture is of PDM microphones only
  if (prv_capture(sai) && (pbl_audio_dai_card(dai)->link->fmt & PBL_AUDIO_FMT_PDM) == 0U) {
    return -ENOTSUP;
  }

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
  const struct pbl_audio_stm32_sai *sai = prv_dev(dai);

  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_rates); i++) {
    caps->rates |= pbl_audio_rate_bit(s_rates[i]);
  }
  caps->min_channels = 1U;
  caps->max_channels = 2U;

  // The filters fill the periods of a capture, so the ring may have any number of them
  if (prv_capture(sai)) {
    caps->gain_min_mdb = 0;
    caps->gain_max_mdb = PBL_PDM_GAIN_MAX_MDB;
    caps->gain_step_mdb = PDM_GAIN_STEP_MDB;
    caps->gain_default_mdb = PDM_GAIN_DEFAULT;
  } else {
    caps->max_periods = 2U;
  }
}

// The kernel clock divided to the nearest of a rate, by up to MCKDIV_MAX
static int prv_divider(const struct pbl_audio_stm32_sai *sai, uint64_t hz, uint32_t *div) {
  uint64_t rate_mhz;
  uint64_t d;
  int ret;

  ret = pbl_clk_get_rate(&sai->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  d = (rate_mhz + hz * MHZ_PER_HZ / 2U) / (hz * MHZ_PER_HZ);
  if (d == 0U || d > MCKDIV_MAX) {
    return -EINVAL;
  }

  *div = (uint32_t)d;

  return 0;
}

static void prv_disable(const struct pbl_audio_stm32_sai *sai) {
  pbl_modifyl(SAI_CR1_DMAEN | SAI_CR1_SAIEN, 0U, sai->base + STM32_SAI_CR1_OFFSET);
  (void)PBL_POLL_BUSY_US((prv_getreg(sai, STM32_SAI_CR1_OFFSET) & SAI_CR1_SAIEN) == 0U, 1U,
                         DISABLE_TIMEOUT_US);
  prv_putreg(sai, STM32_SAI_CR2_OFFSET, SAI_CR2_FFLUSH);
  prv_putreg(sai, STM32_SAI_CLRFR_OFFSET, SAI_CLRFR_ALL);
  if (prv_capture(sai)) {
    pbl_writel(0U, sai->common + STM32_SAI_PDMCR_OFFSET);
  }
}

static int prv_playback_start(const struct pbl_audio_stm32_sai *sai,
                              const struct pbl_audio_stream *stream) {
  size_t period_bytes = stream->period_frames * stream->channels * sizeof(int16_t);
  const struct pbl_dma_slave_config cfg = {
    .dst_addr = sai->base + STM32_SAI_DR_OFFSET,
    .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_2_BYTES,
  };
  uint32_t mckdiv;
  int ret;

  ret = prv_divider(sai, (uint64_t)stream->rate * MCLK_FS, &mckdiv);
  if (ret != 0) {
    return ret;
  }

  prv_disable(sai);

  // Data changes on the falling edge of the bit clock, for the codec to sample on the rising one
  prv_putreg(sai, STM32_SAI_CR1_OFFSET,
             PBL_FIELD_PREP(SAI_CR1_MODE_MASK, SAI_CR1_MODE_MASTER_TX) |
                 PBL_FIELD_PREP(SAI_CR1_DS_MASK, SAI_CR1_DS_16) | SAI_CR1_OUTDRIV |
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

static void prv_pdm_free(const struct pbl_audio_stm32_sai *sai) {
  struct pbl_audio_stm32_sai_state *st = sai->state;

  if (st->ring_allocated) {
    pbl_audio_buf_free(st->ring);
  }
  pbl_free(st->filters);
  st->filters = nullptr;
  st->ring = nullptr;
  st->ring_allocated = false;
}

static int prv_pdm_buffers(const struct pbl_audio_stm32_sai *sai,
                           const struct pbl_audio_stream *stream) {
  struct pbl_audio_stm32_sai_state *st = sai->state;

  st->filters = pbl_malloc(stream->channels * sizeof(struct pbl_pdm_filter));
  if (st->filters == nullptr) {
    return -ENOMEM;
  }

  if (sai->ring != 0U) {
    if (2U * st->half_bytes > sai->ring_size) {
      return -EINVAL;
    }
    st->ring = (uint8_t *)sai->ring;
    return 0;
  }

  st->ring = pbl_audio_buf_alloc(2U * st->half_bytes);
  if (st->ring == nullptr) {
    return -ENOMEM;
  }
  st->ring_allocated = true;

  return 0;
}

// A pair of microphones on the first clock of the PDM interface: the sub-block receives a frame
// of a byte of each per 8 clocks, at twice the clock rate
static int prv_pdm_start(const struct pbl_audio_stm32_sai *sai,
                         const struct pbl_audio_stream *stream, int32_t gain_mdb, bool mute) {
  struct pbl_audio_stm32_sai_state *st = sai->state;
  const struct pbl_dma_slave_config cfg = {
    .src_addr = sai->base + STM32_SAI_DR_OFFSET,
    .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_2_BYTES,
  };
  uint32_t div;
  int ret;

  st->decimation = PDM_DECIMATION;
  while (stream->rate * st->decimation < PDM_CK_MIN_HZ) {
    st->decimation *= 2U;
  }

  ret = prv_divider(sai, 2ULL * stream->rate * st->decimation, &div);
  if (ret != 0) {
    return ret;
  }

  st->stream = *stream;
  st->half_bytes = stream->rate * PDM_HALF_MS / 1000U * 2U * (st->decimation / 8U);
  st->half = 0U;
  st->period = 0U;
  st->frame = 0U;
  st->mute = mute;

  ret = prv_pdm_buffers(sai, stream);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t ch = 0U; ch < stream->channels; ch++) {
    ret =
        pbl_pdm_filter_init(&st->filters[ch], st->decimation, stream->rate * PDM_SETTLE_MS / 1000U);
    if (ret != 0) {
      return ret;
    }
    pbl_pdm_filter_set_gain(&st->filters[ch], gain_mdb);
  }

  prv_disable(sai);

  pbl_writel(PBL_FIELD_PREP(SAI_PDMCR_MICNBR_MASK, 0U) | SAI_PDMCR_CKEN1,
             sai->common + STM32_SAI_PDMCR_OFFSET);
  pbl_modifyl(0U, SAI_PDMCR_PDMEN, sai->common + STM32_SAI_PDMCR_OFFSET);

  prv_putreg(sai, STM32_SAI_CR1_OFFSET,
             PBL_FIELD_PREP(SAI_CR1_MODE_MASK, SAI_CR1_MODE_MASTER_RX) |
                 PBL_FIELD_PREP(SAI_CR1_DS_MASK, SAI_CR1_DS_16) | SAI_CR1_NODIV |
                 PBL_FIELD_PREP(SAI_CR1_MCKDIV_MASK, div));
  prv_putreg(sai, STM32_SAI_CR2_OFFSET, PBL_FIELD_PREP(SAI_CR2_FTH_MASK, SAI_CR2_FTH_QUARTER));
  prv_putreg(sai, STM32_SAI_FRCR_OFFSET,
             PBL_FIELD_PREP(SAI_FRCR_FRL_MASK, PDM_FRAME_BITS - 1U) | SAI_FRCR_FSPOL);
  prv_putreg(sai, STM32_SAI_SLOTR_OFFSET, PBL_FIELD_PREP(SAI_SLOTR_SLOTEN_MASK, PBL_BIT(0)));

  ret = pbl_dma_slave_config(sai->dma, &cfg);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_dma_prep_dma_cyclic(sai->dma, st->ring, 2U * st->half_bytes, st->half_bytes,
                                PBL_DMA_DEV_TO_MEM, prv_pdm_done, (void *)sai);
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
  if (prv_capture(sai)) {
    ret = prv_pdm_start(sai, stream, gain_mdb, mute);
  } else {
    ret = prv_playback_start(sai, stream);
  }

  if (ret != 0) {
    prv_disable(sai);
    (void)pbl_dma_terminate(sai->dma);
    if (prv_capture(sai)) {
      prv_pdm_free(sai);
    }
    pbl_idle_state_release(SOC_STM32_IDLE_STOP);
    (void)pbl_clk_disable(&sai->clk);
  }

  return ret;
}

static int prv_stop(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_stm32_sai *sai = prv_dev(dai);

  prv_disable(sai);
  (void)pbl_dma_terminate(sai->dma);
  if (prv_capture(sai)) {
    prv_pdm_free(sai);
  }
  pbl_idle_state_release(SOC_STM32_IDLE_STOP);

  return pbl_clk_disable(&sai->clk);
}

// The gain and mute of a capture, those of playback being the codec's
static int prv_set_gain(const struct pbl_audio_dai *dai, int32_t gain_mdb) {
  struct pbl_audio_stm32_sai_state *st = prv_dev(dai)->state;

  if (!prv_capture(prv_dev(dai))) {
    return -ENOTSUP;
  }

  pbl_irq_lock();
  for (uint8_t ch = 0U; st->filters != nullptr && ch < st->stream.channels; ch++) {
    pbl_pdm_filter_set_gain(&st->filters[ch], gain_mdb);
  }
  pbl_irq_unlock();

  return 0;
}

static int prv_set_mute(const struct pbl_audio_dai *dai, bool mute) {
  if (!prv_capture(prv_dev(dai))) {
    return -ENOTSUP;
  }

  prv_dev(dai)->state->mute = mute;

  return 0;
}

static void prv_sync_for_device(const struct pbl_audio_dai *dai, void *buf, size_t len) {
  pbl_dcache_flush_range(buf, len);
}

const struct pbl_audio_dai_ops pbl_audio_stm32_sai_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .set_gain = prv_set_gain,
  .set_mute = prv_set_mute,
  .sync_for_device = prv_sync_for_device,
};

#include PBL_DT_INSTANCES(st_stm32_sai_sub_a)
