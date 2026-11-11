/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf52_pdm.h>

// Gain register: 0.5 dB steps from -20 dB
#define GAIN_MIN_MDB  (-20000)
#define GAIN_MAX_MDB  20000
#define GAIN_STEP_MDB 500

#define STOP_TIMEOUT_MS 50U

static inline uint32_t prv_getreg(const struct pbl_audio_nrf5_pdm *pdm, uint32_t offset) {
  return pbl_readl(pdm->base + offset);
}

static inline void prv_putreg(const struct pbl_audio_nrf5_pdm *pdm, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, pdm->base + offset);
}

static void prv_event_clear(const struct pbl_audio_nrf5_pdm *pdm, uint32_t offset) {
  prv_putreg(pdm, offset, 0U);
  (void)prv_getreg(pdm, offset);
}

static const struct pbl_audio_nrf5_pdm *prv_dev(const struct pbl_audio_dai *dai) {
  return container_of(dai, const struct pbl_audio_nrf5_pdm, dai);
}

static uint32_t prv_gain(int32_t gain_mdb) {
  return (uint32_t)pbl_audio_gain_to_step(gain_mdb, GAIN_MIN_MDB, GAIN_STEP_MDB);
}

// Without a clock, the microphone leaves the data line floating
static int prv_din_idle(const struct pbl_audio_nrf5_pdm *pdm) {
  return pbl_gpio_configure(&pdm->din, PBL_GPIO_INPUT | PBL_GPIO_PULL_DOWN);
}

static void prv_queue(const struct pbl_audio_nrf5_pdm *pdm) {
  struct pbl_audio_nrf5_pdm_state *state = pdm->state;

  prv_putreg(pdm, NRF52_PDM_SAMPLE_PTR_OFFSET,
             (uint32_t)(uintptr_t)pbl_audio_period(pbl_audio_dai_card(&pdm->dai), state->next));
  state->next = (state->next + 1U) % pbl_audio_dai_card(&pdm->dai)->state->stream.periods;
}

void pbl_audio_nrf5_pdm_irq_handler(const struct pbl_audio_nrf5_pdm *pdm) {
  struct pbl_audio_nrf5_pdm_state *state = pdm->state;

  if (prv_getreg(pdm, NRF52_PDM_EVENTS_STARTED_OFFSET) == 0U) {
    return;
  }

  prv_event_clear(pdm, NRF52_PDM_EVENTS_STARTED_OFFSET);

  // Each start latches the queued period, so the one before it is complete
  prv_queue(pdm);
  if (state->started) {
    pbl_audio_period_elapsed(pbl_audio_dai_card(&pdm->dai));
  }
  state->started = true;
}

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_nrf5_pdm *pdm = prv_dev(dai);
  int ret;

  if (pbl_device_init(&pdm->clk.port->dev) != 0 || pbl_device_init(&pdm->din.port->dev) != 0) {
    return -ENODEV;
  }

  ret = pbl_gpio_configure(&pdm->clk, PBL_GPIO_OUTPUT_LOW);
  if (ret != 0) {
    return ret;
  }

  ret = prv_din_idle(pdm);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(pdm, NRF52_PDM_INTENSET_OFFSET, PDM_INT_STARTED);
  pbl_irq_enable(pdm->irq);

  return 0;
}

static void prv_get_caps(const struct pbl_audio_dai *dai, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 2U;
  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = 0;
}

static int prv_start(const struct pbl_audio_dai *dai, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_nrf5_pdm *pdm = prv_dev(dai);
  const uint32_t samples = stream->period_frames * stream->channels;
  const uint32_t gain = prv_gain(mute ? GAIN_MIN_MDB : gain_mdb);
  int ret;

  if (samples > PDM_SAMPLE_MAXCNT_MAX) {
    return -EINVAL;
  }

  ret = pbl_clk_enable(&pdm->hfclk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&pdm->din, PBL_GPIO_INPUT);
  if (ret != 0) {
    (void)pbl_clk_disable(&pdm->hfclk);
    return ret;
  }

  pdm->state->next = 0U;
  pdm->state->started = false;

  prv_putreg(pdm, NRF52_PDM_PSEL_CLK_OFFSET, pbl_gpio_nrf5_pin(&pdm->clk));
  prv_putreg(pdm, NRF52_PDM_PSEL_DIN_OFFSET, pbl_gpio_nrf5_pin(&pdm->din));
  prv_putreg(pdm, NRF52_PDM_PDMCLKCTRL_OFFSET, PDM_PDMCLKCTRL_FREQ_1280K);
  prv_putreg(pdm, NRF52_PDM_RATIO_OFFSET, PDM_RATIO_80);
  prv_putreg(pdm, NRF52_PDM_MODE_OFFSET, stream->channels == 1U ? PDM_MODE_MONO : 0U);
  prv_putreg(pdm, NRF52_PDM_GAINL_OFFSET, gain);
  prv_putreg(pdm, NRF52_PDM_GAINR_OFFSET, gain);
  prv_putreg(pdm, NRF52_PDM_SAMPLE_MAXCNT_OFFSET, samples);
  prv_queue(pdm);

  prv_event_clear(pdm, NRF52_PDM_EVENTS_STARTED_OFFSET);
  prv_event_clear(pdm, NRF52_PDM_EVENTS_STOPPED_OFFSET);

  prv_putreg(pdm, NRF52_PDM_ENABLE_OFFSET, PDM_ENABLE_ENABLED);
  prv_putreg(pdm, NRF52_PDM_TASKS_START_OFFSET, 1U);

  return 0;
}

static int prv_stop(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_nrf5_pdm *pdm = prv_dev(dai);
  int ret;

  prv_putreg(pdm, NRF52_PDM_TASKS_STOP_OFFSET, 1U);
  ret = PBL_POLL_SLEEP_MS(prv_getreg(pdm, NRF52_PDM_EVENTS_STOPPED_OFFSET) != 0U, 1U,
                          STOP_TIMEOUT_MS);

  prv_putreg(pdm, NRF52_PDM_ENABLE_OFFSET, 0U);
  prv_putreg(pdm, NRF52_PDM_PSEL_CLK_OFFSET, PDM_PSEL_DISCONNECTED);
  prv_putreg(pdm, NRF52_PDM_PSEL_DIN_OFFSET, PDM_PSEL_DISCONNECTED);
  prv_event_clear(pdm, NRF52_PDM_EVENTS_STARTED_OFFSET);
  prv_event_clear(pdm, NRF52_PDM_EVENTS_STOPPED_OFFSET);
  pbl_irq_clear_pending(pdm->irq);

  (void)pbl_clk_disable(&pdm->hfclk);

  (void)prv_din_idle(pdm);

  return ret;
}

static int prv_set_gain(const struct pbl_audio_dai *dai, int32_t gain_mdb) {
  const struct pbl_audio_nrf5_pdm *pdm = prv_dev(dai);

  prv_putreg(pdm, NRF52_PDM_GAINL_OFFSET, prv_gain(gain_mdb));
  prv_putreg(pdm, NRF52_PDM_GAINR_OFFSET, prv_gain(gain_mdb));

  return 0;
}

const struct pbl_audio_dai_ops pbl_audio_nrf5_pdm_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .set_gain = prv_set_gain,
};

#include PBL_DT_INSTANCES(nordic_nrf52832_pdm)
