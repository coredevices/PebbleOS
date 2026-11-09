/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/clocksource.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf52_i2s.h>

#define STEREO       2U
#define BOUNCE_BYTES (PBL_AUDIO_NRF5_I2S_BOUNCE_FRAMES * STEREO * sizeof(int16_t))

#define STOP_TIMEOUT_MS 50U

// nRF52 anomaly 194: the STOP task leaves resources on unless these are written too
#define ANOMALY_194_OFFSET0 0x0038U
#define ANOMALY_194_OFFSET1 0x003cU

static inline uint32_t prv_getreg(const struct pbl_audio_nrf5_i2s *i2s, uint32_t offset) {
  return pbl_readl(i2s->base + offset);
}

static inline void prv_putreg(const struct pbl_audio_nrf5_i2s *i2s, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, i2s->base + offset);
}

static void prv_event_clear(const struct pbl_audio_nrf5_i2s *i2s, uint32_t offset) {
  prv_putreg(i2s, offset, 0U);
  (void)prv_getreg(i2s, offset);
}

static const struct pbl_audio_nrf5_i2s *prv_dev(const struct pbl_audio_dai *dai) {
  return container_of(dai, const struct pbl_audio_nrf5_i2s, dai);
}

static bool prv_slave(const struct pbl_audio_nrf5_i2s *i2s) {
  return (pbl_audio_dai_card(&i2s->dai)->link->fmt & PBL_AUDIO_FMT_CODEC_MASTER) != 0U;
}

// As a slave, the clocks come from the codec, which leaves them floating when idle
static int prv_clocks_configure(const struct pbl_audio_nrf5_i2s *i2s, bool idle) {
  const uint32_t flags =
      prv_slave(i2s) ? (PBL_GPIO_INPUT | (idle ? PBL_GPIO_PULL_DOWN : 0U)) : PBL_GPIO_OUTPUT_LOW;
  int ret;

  ret = pbl_gpio_configure(&i2s->sck, flags);
  if (ret != 0) {
    return ret;
  }

  return pbl_gpio_configure(&i2s->lrck, flags);
}

// Send the next period of the ring as stereo, then hand it back to the client
static void prv_fill(const struct pbl_audio_nrf5_i2s *i2s) {
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  const struct pbl_audio_state *audio_state = pbl_audio_dai_card(&i2s->dai)->state;
  const size_t frames = audio_state->stream.period_frames;
  const int16_t *period = pbl_audio_period(pbl_audio_dai_card(&i2s->dai), state->ring_idx);
  int16_t *out = state->bounce[state->bounce_idx];

  for (size_t i = 0U; i < frames; i++) {
    out[STEREO * i] = period[i];
    out[STEREO * i + 1U] = period[i];
  }

  prv_putreg(i2s, NRF52_I2S_TXD_PTR_OFFSET, (uint32_t)(uintptr_t)out);

  state->ring_idx = (state->ring_idx + 1U) % audio_state->stream.periods;
  state->bounce_idx ^= 1U;

  pbl_audio_period_elapsed(pbl_audio_dai_card(&i2s->dai));
}

void pbl_audio_nrf5_i2s_irq_handler(const struct pbl_audio_nrf5_i2s *i2s) {
  if (prv_getreg(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET) == 0U) {
    return;
  }

  prv_event_clear(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);

  // The queued buffer is now being sent: queue the next one
  prv_fill(i2s);
}

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(dai);
  int ret;

  if (pbl_audio_dai_card(dai)->link->codec == nullptr) {
    return -EINVAL;
  }

  if (pbl_device_init(&i2s->sck.port->dev) != 0 || pbl_device_init(&i2s->lrck.port->dev) != 0 ||
      pbl_device_init(&i2s->mck.port->dev) != 0 || pbl_device_init(&i2s->sdout.port->dev) != 0) {
    return -ENODEV;
  }

  ret = prv_clocks_configure(i2s, true);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&i2s->mck, PBL_GPIO_OUTPUT_LOW);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&i2s->sdout, PBL_GPIO_OUTPUT_LOW);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(i2s->irq);

  return 0;
}

static void prv_get_caps(const struct pbl_audio_dai *dai, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 1U;
}

static void prv_free(struct pbl_audio_nrf5_i2s_state *state) {
  for (size_t i = 0U; i < ARRAY_LENGTH(state->bounce); i++) {
    pbl_free(state->bounce[i]);
    state->bounce[i] = nullptr;
  }
}

static int prv_start(const struct pbl_audio_dai *dai, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(dai);
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  int ret;

  if (stream->period_frames > PBL_AUDIO_NRF5_I2S_BOUNCE_FRAMES) {
    return -EINVAL;
  }

  *state = (struct pbl_audio_nrf5_i2s_state){0};
  for (size_t i = 0U; i < ARRAY_LENGTH(state->bounce); i++) {
    state->bounce[i] = pbl_malloc(BOUNCE_BYTES);
    if (state->bounce[i] == nullptr) {
      prv_free(state);
      return -ENOMEM;
    }
  }

  ret = prv_clocks_configure(i2s, false);
  if (ret != 0) {
    prv_free(state);
    return ret;
  }

  prv_putreg(i2s, NRF52_I2S_PSEL_MCK_OFFSET, pbl_gpio_nrf5_pin(&i2s->mck));
  prv_putreg(i2s, NRF52_I2S_PSEL_SCK_OFFSET, pbl_gpio_nrf5_pin(&i2s->sck));
  prv_putreg(i2s, NRF52_I2S_PSEL_LRCK_OFFSET, pbl_gpio_nrf5_pin(&i2s->lrck));
  prv_putreg(i2s, NRF52_I2S_PSEL_SDIN_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_SDOUT_OFFSET, pbl_gpio_nrf5_pin(&i2s->sdout));

  prv_putreg(i2s, NRF52_I2S_CONFIG_MODE_OFFSET,
             prv_slave(i2s) ? I2S_CONFIG_MODE_SLAVE : I2S_CONFIG_MODE_MASTER);
  prv_putreg(i2s, NRF52_I2S_CONFIG_RXEN_OFFSET, 0U);
  prv_putreg(i2s, NRF52_I2S_CONFIG_TXEN_OFFSET, I2S_CONFIG_EN);
  // The 4 MHz master clock feeds the codec PLL, which derives an exact 16 kHz frame clock
  prv_putreg(i2s, NRF52_I2S_CONFIG_MCKEN_OFFSET, I2S_CONFIG_EN);
  prv_putreg(i2s, NRF52_I2S_CONFIG_MCKFREQ_OFFSET, I2S_CONFIG_MCKFREQ_32MDIV8);
  prv_putreg(i2s, NRF52_I2S_CONFIG_RATIO_OFFSET, I2S_CONFIG_RATIO_256X);
  prv_putreg(i2s, NRF52_I2S_CONFIG_SWIDTH_OFFSET, I2S_CONFIG_SWIDTH_16BIT);
  prv_putreg(i2s, NRF52_I2S_CONFIG_ALIGN_OFFSET, I2S_CONFIG_ALIGN_LEFT);
  prv_putreg(i2s, NRF52_I2S_CONFIG_FORMAT_OFFSET, I2S_CONFIG_FORMAT_I2S);
  prv_putreg(i2s, NRF52_I2S_CONFIG_CHANNELS_OFFSET, I2S_CONFIG_CHANNELS_STEREO);
  prv_putreg(i2s, NRF52_I2S_RXTXD_MAXCNT_OFFSET,
             stream->period_frames * STEREO * sizeof(int16_t) / sizeof(uint32_t));

  prv_fill(i2s);

  clocksource_hfxo_request();

  prv_putreg(i2s, NRF52_I2S_ENABLE_OFFSET, I2S_ENABLE_ENABLED);
  // nRF52 anomaly 55: enabling may raise spurious pointer update events
  prv_event_clear(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_STOPPED_OFFSET);
  prv_putreg(i2s, NRF52_I2S_INTENSET_OFFSET, I2S_INT_TXPTRUPD);
  prv_putreg(i2s, NRF52_I2S_TASKS_START_OFFSET, 1U);

  return 0;
}

static int prv_stop(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(dai);
  int ret;

  // Mask the pointer updates first, which may be spurious once stopping (nRF52 anomaly 55)
  prv_putreg(i2s, NRF52_I2S_INTENCLR_OFFSET, I2S_INT_TXPTRUPD);
  prv_putreg(i2s, NRF52_I2S_TASKS_STOP_OFFSET, 1U);
  prv_putreg(i2s, ANOMALY_194_OFFSET0, 1U);
  prv_putreg(i2s, ANOMALY_194_OFFSET1, 1U);

  ret = PBL_POLL_SLEEP_MS(prv_getreg(i2s, NRF52_I2S_EVENTS_STOPPED_OFFSET) != 0U, 1U,
                          STOP_TIMEOUT_MS);

  prv_putreg(i2s, NRF52_I2S_ENABLE_OFFSET, 0U);
  // nRF52 anomaly 196: disabling does not release the pins
  prv_putreg(i2s, NRF52_I2S_PSEL_MCK_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_SCK_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_LRCK_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_SDOUT_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_STOPPED_OFFSET);
  pbl_irq_clear_pending(i2s->irq);

  clocksource_hfxo_release();
  prv_free(i2s->state);

  (void)prv_clocks_configure(i2s, true);

  return ret;
}

const struct pbl_audio_dai_ops pbl_audio_nrf5_i2s_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
};

#include PBL_DT_INSTANCES(nordic_nrf52832_i2s)
