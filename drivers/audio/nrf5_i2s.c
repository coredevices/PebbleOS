/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/clocksource.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/util/misc.h>

#include <kernel/pbl_malloc.h>
#include <nrfx_i2s.h>

#define STEREO       2U
#define BOUNCE_BYTES (PBL_AUDIO_NRF5_I2S_BOUNCE_FRAMES * STEREO * sizeof(int16_t))

static const nrfx_i2s_t s_i2s = NRFX_I2S_INSTANCE(0);
static const struct pbl_audio_nrf5_i2s *s_running;

static const struct pbl_audio_nrf5_i2s *prv_dev(const struct pbl_audio *audio) {
  return container_of(audio, const struct pbl_audio_nrf5_i2s, audio);
}

// Send the next period of the ring as stereo, then hand it back to the client
static nrfx_i2s_buffers_t prv_fill(const struct pbl_audio_nrf5_i2s *i2s) {
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  const struct pbl_audio_state *audio_state = i2s->audio.state;
  size_t frames = audio_state->stream.period_frames;
  const int16_t *period = pbl_audio_period(&i2s->audio, state->ring_idx);
  int16_t *out = state->bounce[state->bounce_idx];

  for (size_t i = 0U; i < frames; i++) {
    out[STEREO * i] = period[i];
    out[STEREO * i + 1U] = period[i];
  }

  state->ring_idx = (state->ring_idx + 1U) % audio_state->stream.periods;
  state->bounce_idx ^= 1U;

  return (nrfx_i2s_buffers_t){
    .p_tx_buffer = (uint32_t *)out,
    .buffer_size = (uint32_t)(frames * STEREO * sizeof(int16_t) / sizeof(uint32_t)),
  };
}

static void prv_data_handler(nrfx_i2s_buffers_t const *released, uint32_t status) {
  const struct pbl_audio_nrf5_i2s *i2s = s_running;
  nrfx_i2s_buffers_t next;

  if (i2s == nullptr || (status & NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED) == 0U) {
    return;
  }

  next = prv_fill(i2s);
  (void)nrfx_i2s_next_buffers_set(&s_i2s, &next);
  pbl_audio_period_elapsed(&i2s->audio);
}

static void prv_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 1U;
}

static void prv_free(struct pbl_audio_nrf5_i2s_state *state) {
  for (size_t i = 0U; i < ARRAY_LENGTH(state->bounce); i++) {
    kernel_free(state->bounce[i]);
    state->bounce[i] = nullptr;
  }
}

static int prv_hw_start(const struct pbl_audio_nrf5_i2s *i2s) {
  bool slave = (i2s->audio.link->fmt & PBL_AUDIO_FMT_CODEC_MASTER) != 0U;
  nrfx_i2s_config_t cfg = NRFX_I2S_DEFAULT_CONFIG(
      pbl_gpio_nrf5_pin(&i2s->sck), pbl_gpio_nrf5_pin(&i2s->lrck), pbl_gpio_nrf5_pin(&i2s->mck),
      pbl_gpio_nrf5_pin(&i2s->sdout), NRF_I2S_PIN_NOT_CONNECTED);
  nrfx_i2s_buffers_t initial;

  cfg.irq_priority = i2s->irq_priority;
  cfg.channels = NRF_I2S_CHANNELS_STEREO;
  cfg.sample_width = NRF_I2S_SWIDTH_16BIT;
  cfg.format = NRF_I2S_FORMAT_I2S;
  cfg.alignment = NRF_I2S_ALIGN_LEFT;
  // The 4 MHz master clock feeds the codec PLL, which derives an exact 16 kHz frame clock
  cfg.mode = slave ? NRF_I2S_MODE_SLAVE : NRF_I2S_MODE_MASTER;
  cfg.mck_setup = NRF_I2S_MCK_32MDIV8;
  cfg.ratio = NRF_I2S_RATIO_256X;

  if (nrfx_i2s_init(&s_i2s, &cfg, prv_data_handler) != NRFX_SUCCESS) {
    return -EIO;
  }

  initial = prv_fill(i2s);
  pbl_audio_period_elapsed(&i2s->audio);

  if (nrfx_i2s_start(&s_i2s, &initial, 0U) != NRFX_SUCCESS) {
    nrfx_i2s_uninit(&s_i2s);
    return -EIO;
  }

  return 0;
}

static int prv_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(audio);
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  int ret;

  if (stream->period_frames > PBL_AUDIO_NRF5_I2S_BOUNCE_FRAMES || i2s->audio.link == nullptr) {
    return -EINVAL;
  }

  *state = (struct pbl_audio_nrf5_i2s_state){0};
  for (size_t i = 0U; i < ARRAY_LENGTH(state->bounce); i++) {
    state->bounce[i] = kernel_malloc(BOUNCE_BYTES);
    if (state->bounce[i] == nullptr) {
      prv_free(state);
      return -ENOMEM;
    }
  }

  s_running = i2s;
  clocksource_hfxo_request();

  ret = prv_hw_start(i2s);
  if (ret != 0) {
    clocksource_hfxo_release();
    s_running = nullptr;
    prv_free(state);
  }

  return ret;
}

static int prv_stop(const struct pbl_audio *audio) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(audio);

  nrfx_i2s_stop(&s_i2s);
  nrfx_i2s_uninit(&s_i2s);
  s_running = nullptr;
  clocksource_hfxo_release();
  prv_free(i2s->state);

  return 0;
}

const struct pbl_audio_ops pbl_audio_nrf5_i2s_ops = {
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
};
