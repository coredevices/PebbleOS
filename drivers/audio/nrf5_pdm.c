/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/clocksource.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/misc.h>

#include <nrfx_pdm.h>

// Gain register: 0.5 dB steps from -20 dB
#define GAIN_MIN_MDB  (-20000)
#define GAIN_MAX_MDB  20000
#define GAIN_STEP_MDB 500

// The peripheral may still write the buffers this long after a stop
#define STOP_SETTLE_MS 1U

static const nrfx_pdm_t s_pdm = NRFX_PDM_INSTANCE(0);
static const struct pbl_audio_nrf5_pdm *s_running;

static const struct pbl_audio_nrf5_pdm *prv_dev(const struct pbl_audio *audio) {
  return container_of(audio, const struct pbl_audio_nrf5_pdm, audio);
}

static nrf_pdm_gain_t prv_gain(int32_t gain_mdb) {
  return (nrf_pdm_gain_t)pbl_audio_gain_to_step(gain_mdb, GAIN_MIN_MDB, GAIN_STEP_MDB);
}

static void prv_event_handler(nrfx_pdm_evt_t const *evt) {
  const struct pbl_audio_nrf5_pdm *pdm = s_running;
  const struct pbl_audio_state *audio_state;

  if (pdm == nullptr) {
    return;
  }

  audio_state = pdm->audio.state;

  if (evt->error != NRFX_PDM_NO_ERROR) {
    pbl_audio_report_xrun(&pdm->audio);
  }

  if (evt->buffer_requested) {
    (void)nrfx_pdm_buffer_set(&s_pdm, pbl_audio_period(&pdm->audio, pdm->state->next),
                              audio_state->stream.period_frames * audio_state->stream.channels);
    pdm->state->next = (pdm->state->next + 1U) % audio_state->stream.periods;
  }

  if (evt->buffer_released != nullptr) {
    pbl_audio_period_elapsed(&pdm->audio);
  }
}

static void prv_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 2U;
  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = prv_dev(audio)->default_gain_mdb;
}

static int prv_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_nrf5_pdm *pdm = prv_dev(audio);
  nrfx_pdm_config_t cfg =
      NRFX_PDM_DEFAULT_CONFIG(pbl_gpio_nrf5_pin(&pdm->clk), pbl_gpio_nrf5_pin(&pdm->din));

  cfg.mode = stream->channels == 1U ? NRF_PDM_MODE_MONO : NRF_PDM_MODE_STEREO;
  cfg.clock_freq = NRF_PDM_FREQ_1280K;
  cfg.ratio = NRF_PDM_RATIO_80X;
  cfg.gain_l = prv_gain(mute ? GAIN_MIN_MDB : gain_mdb);
  cfg.gain_r = cfg.gain_l;

  pdm->state->next = 0U;
  s_running = pdm;

  if (nrfx_pdm_init(&s_pdm, &cfg, prv_event_handler) != NRFX_SUCCESS) {
    s_running = nullptr;
    return -EIO;
  }

  clocksource_hfxo_request();

  if (nrfx_pdm_start(&s_pdm) != NRFX_SUCCESS) {
    clocksource_hfxo_release();
    nrfx_pdm_uninit(&s_pdm);
    s_running = nullptr;
    return -EIO;
  }

  return 0;
}

static int prv_stop(const struct pbl_audio *audio) {
  (void)nrfx_pdm_stop(&s_pdm);
  pbl_sleep_ms(STOP_SETTLE_MS);
  nrfx_pdm_uninit(&s_pdm);
  s_running = nullptr;
  clocksource_hfxo_release();

  return 0;
}

static int prv_set_gain(const struct pbl_audio *audio, int32_t gain_mdb) {
  nrf_pdm_gain_set(s_pdm.p_reg, prv_gain(gain_mdb), prv_gain(gain_mdb));

  return 0;
}

const struct pbl_audio_ops pbl_audio_nrf5_pdm_ops = {
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .set_gain = prv_set_gain,
};
