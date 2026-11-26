/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/nrf52_i2s.h>

#define STEREO      2U
#define FRAME_BYTES (STEREO * sizeof(int16_t))

// A frame is 62.5 us at 16 kHz: with the clocks running, the hardware stops within a few
#define STOP_BUSY_US    250U
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
  return PBL_CONTAINER_OF(dai, const struct pbl_audio_nrf5_i2s, dai);
}

static const struct pbl_audio *prv_card(const struct pbl_audio_nrf5_i2s *i2s) {
  return pbl_audio_dai_card(&i2s->dai);
}

static const struct pbl_gpio *prv_data_pin(const struct pbl_audio_nrf5_i2s *i2s,
                                           enum pbl_audio_dir dir) {
  return dir == PBL_AUDIO_PLAYBACK ? &i2s->sdout : &i2s->sdin;
}

static bool prv_slave(const struct pbl_audio_nrf5_i2s *i2s) {
  return (prv_card(i2s)->link->fmt & PBL_AUDIO_FMT_CODEC_MASTER) != 0U;
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

// Send the next frames of the ring as stereo, handing each period back once sent
static void prv_tx_fill(const struct pbl_audio_nrf5_i2s *i2s, int16_t *out) {
  struct pbl_audio_nrf5_i2s_dir *s = &i2s->state->dirs[PBL_AUDIO_PLAYBACK];
  const struct pbl_audio *card = prv_card(i2s);
  const struct pbl_audio_stream *stream = pbl_audio_stream(card, PBL_AUDIO_PLAYBACK);
  const size_t block = i2s->state->block_frames;
  size_t done = 0U;

  while (done < block) {
    const int16_t *period = pbl_audio_period(card, PBL_AUDIO_PLAYBACK, s->ring_idx);
    const size_t n = PBL_MIN(block - done, stream->period_frames - s->offset);

    for (size_t i = 0U; i < n; i++) {
      out[STEREO * (done + i)] = period[s->offset + i];
      out[STEREO * (done + i) + 1U] = period[s->offset + i];
    }

    done += n;
    s->offset += n;
    if (s->offset == stream->period_frames) {
      s->offset = 0U;
      s->ring_idx = (s->ring_idx + 1U) % stream->periods;
      pbl_audio_period_elapsed(card, PBL_AUDIO_PLAYBACK);
    }
  }
}

// Copy a recorded block into the ring, handing out each period once full
static void prv_rx_drain(const struct pbl_audio_nrf5_i2s *i2s, const int16_t *in) {
  struct pbl_audio_nrf5_i2s_dir *s = &i2s->state->dirs[PBL_AUDIO_CAPTURE];
  const struct pbl_audio *card = prv_card(i2s);
  const struct pbl_audio_stream *stream = pbl_audio_stream(card, PBL_AUDIO_CAPTURE);
  const size_t block = i2s->state->block_frames;
  size_t done = 0U;

  while (done < block) {
    int16_t *period = pbl_audio_period(card, PBL_AUDIO_CAPTURE, s->ring_idx);
    const size_t n = PBL_MIN(block - done, stream->period_frames - s->offset);

    if (stream->channels == STEREO) {
      memcpy(&period[STEREO * s->offset], &in[STEREO * done], n * FRAME_BYTES);
    } else {
      for (size_t i = 0U; i < n; i++) {
        period[s->offset + i] = in[STEREO * (done + i)];
      }
    }

    done += n;
    s->offset += n;
    if (s->offset == stream->period_frames) {
      s->offset = 0U;
      s->ring_idx = (s->ring_idx + 1U) % stream->periods;
      pbl_audio_period_elapsed(card, PBL_AUDIO_CAPTURE);
    }
  }
}

// The block queued last is now being sent: fill and queue the next
static void prv_tx_queue(const struct pbl_audio_nrf5_i2s *i2s) {
  struct pbl_audio_nrf5_i2s_dir *s = &i2s->state->dirs[PBL_AUDIO_PLAYBACK];
  int16_t *out = s->bounce[s->bounce_idx];

  prv_tx_fill(i2s, out);
  prv_putreg(i2s, NRF52_I2S_TXD_PTR_OFFSET, (uint32_t)(uintptr_t)out);
  s->bounce_idx ^= 1U;
}

// The block written before the one now starting is complete, unless this is the first: deliver
// it, and queue its buffer again for the next block
static void prv_rx_queue(const struct pbl_audio_nrf5_i2s *i2s) {
  struct pbl_audio_nrf5_i2s_dir *s = &i2s->state->dirs[PBL_AUDIO_CAPTURE];
  int16_t *in = s->bounce[s->bounce_idx];

  if (s->started) {
    prv_rx_drain(i2s, in);
  }
  s->started = true;

  prv_putreg(i2s, NRF52_I2S_RXD_PTR_OFFSET, (uint32_t)(uintptr_t)in);
  s->bounce_idx ^= 1U;
}

void pbl_audio_nrf5_i2s_irq_handler(const struct pbl_audio_nrf5_i2s *i2s) {
  const uint8_t running = i2s->state->running;

  if (prv_getreg(i2s, NRF52_I2S_EVENTS_RXPTRUPD_OFFSET) != 0U) {
    prv_event_clear(i2s, NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
    if ((running & PBL_BIT(PBL_AUDIO_CAPTURE)) != 0U) {
      prv_rx_queue(i2s);
    }
  }

  if (prv_getreg(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET) != 0U) {
    prv_event_clear(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
    if ((running & PBL_BIT(PBL_AUDIO_PLAYBACK)) != 0U) {
      prv_tx_queue(i2s);
    }
  }
}

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(dai);
  int ret;

  if (prv_card(i2s)->link->codec == nullptr ||
      (i2s->sdout.port == nullptr && i2s->sdin.port == nullptr)) {
    return -EINVAL;
  }

  if (pbl_device_init(&i2s->sck.port->dev) != 0 || pbl_device_init(&i2s->lrck.port->dev) != 0 ||
      pbl_device_init(&i2s->mck.port->dev) != 0) {
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

  if (i2s->sdout.port != nullptr) {
    if (pbl_device_init(&i2s->sdout.port->dev) != 0) {
      return -ENODEV;
    }

    ret = pbl_gpio_configure(&i2s->sdout, PBL_GPIO_OUTPUT_LOW);
    if (ret != 0) {
      return ret;
    }
  }

  // The codec leaves its data output floating when not recording
  if (i2s->sdin.port != nullptr) {
    if (pbl_device_init(&i2s->sdin.port->dev) != 0) {
      return -ENODEV;
    }

    ret = pbl_gpio_configure(&i2s->sdin, PBL_GPIO_INPUT | PBL_GPIO_PULL_DOWN);
    if (ret != 0) {
      return ret;
    }
  }

  pbl_irq_enable(i2s->irq);

  return 0;
}

// A direction needs its data pin
static void prv_get_caps(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir,
                         struct pbl_audio_caps *caps) {
  if (prv_data_pin(prv_dev(dai), dir)->port == nullptr) {
    return;
  }

  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = dir == PBL_AUDIO_PLAYBACK ? 1U : STEREO;
}

static void prv_free(struct pbl_audio_nrf5_i2s_dir *s) {
  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s->bounce); i++) {
    pbl_free(s->bounce[i]);
  }
  *s = (struct pbl_audio_nrf5_i2s_dir){0};
}

static int prv_alloc(struct pbl_audio_nrf5_i2s_dir *s, size_t block_frames) {
  *s = (struct pbl_audio_nrf5_i2s_dir){0};

  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s->bounce); i++) {
    s->bounce[i] = pbl_malloc(block_frames * FRAME_BYTES);
    if (s->bounce[i] == nullptr) {
      prv_free(s);
      return -ENOMEM;
    }
  }

  return 0;
}

// Configure and start the directions in dirs, which cannot change while the hardware runs. A
// playback continuing from before sends first the block it had queued, unless the hardware took
// it already; a capture continuing from before starts a new block.
static void prv_hw_run(const struct pbl_audio_nrf5_i2s *i2s, uint8_t dirs, bool resend) {
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  const bool tx = (dirs & PBL_BIT(PBL_AUDIO_PLAYBACK)) != 0U;
  const bool rx = (dirs & PBL_BIT(PBL_AUDIO_CAPTURE)) != 0U;
  uint32_t inten = 0U;

  prv_putreg(i2s, NRF52_I2S_PSEL_MCK_OFFSET, pbl_gpio_nrf5_pin(&i2s->mck));
  prv_putreg(i2s, NRF52_I2S_PSEL_SCK_OFFSET, pbl_gpio_nrf5_pin(&i2s->sck));
  prv_putreg(i2s, NRF52_I2S_PSEL_LRCK_OFFSET, pbl_gpio_nrf5_pin(&i2s->lrck));
  prv_putreg(i2s, NRF52_I2S_PSEL_SDIN_OFFSET,
             rx ? pbl_gpio_nrf5_pin(&i2s->sdin) : I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_SDOUT_OFFSET,
             tx ? pbl_gpio_nrf5_pin(&i2s->sdout) : I2S_PSEL_DISCONNECTED);

  prv_putreg(i2s, NRF52_I2S_CONFIG_MODE_OFFSET,
             prv_slave(i2s) ? I2S_CONFIG_MODE_SLAVE : I2S_CONFIG_MODE_MASTER);
  prv_putreg(i2s, NRF52_I2S_CONFIG_RXEN_OFFSET, rx ? I2S_CONFIG_EN : 0U);
  prv_putreg(i2s, NRF52_I2S_CONFIG_TXEN_OFFSET, tx ? I2S_CONFIG_EN : 0U);
  // The 4 MHz master clock feeds the codec PLL, which derives an exact 16 kHz frame clock
  prv_putreg(i2s, NRF52_I2S_CONFIG_MCKEN_OFFSET, I2S_CONFIG_EN);
  prv_putreg(i2s, NRF52_I2S_CONFIG_MCKFREQ_OFFSET, I2S_CONFIG_MCKFREQ_32MDIV8);
  prv_putreg(i2s, NRF52_I2S_CONFIG_RATIO_OFFSET, I2S_CONFIG_RATIO_256X);
  prv_putreg(i2s, NRF52_I2S_CONFIG_SWIDTH_OFFSET, I2S_CONFIG_SWIDTH_16BIT);
  prv_putreg(i2s, NRF52_I2S_CONFIG_ALIGN_OFFSET, I2S_CONFIG_ALIGN_LEFT);
  prv_putreg(i2s, NRF52_I2S_CONFIG_FORMAT_OFFSET, I2S_CONFIG_FORMAT_I2S);
  prv_putreg(i2s, NRF52_I2S_CONFIG_CHANNELS_OFFSET, I2S_CONFIG_CHANNELS_STEREO);
  // In 32-bit words, one per stereo frame
  prv_putreg(i2s, NRF52_I2S_RXTXD_MAXCNT_OFFSET, state->block_frames);

  if (tx) {
    struct pbl_audio_nrf5_i2s_dir *s = &state->dirs[PBL_AUDIO_PLAYBACK];

    if (resend) {
      prv_putreg(i2s, NRF52_I2S_TXD_PTR_OFFSET, (uint32_t)(uintptr_t)s->bounce[s->bounce_idx ^ 1U]);
    } else {
      prv_tx_queue(i2s);
    }
    inten |= I2S_INT_TXPTRUPD;
  }

  if (rx) {
    struct pbl_audio_nrf5_i2s_dir *s = &state->dirs[PBL_AUDIO_CAPTURE];

    prv_putreg(i2s, NRF52_I2S_RXD_PTR_OFFSET, (uint32_t)(uintptr_t)s->bounce[0]);
    s->bounce_idx = 1U;
    s->started = false;
    inten |= I2S_INT_RXPTRUPD;
  }

  state->running = dirs;

  prv_putreg(i2s, NRF52_I2S_ENABLE_OFFSET, I2S_ENABLE_ENABLED);
  // nRF52 anomaly 55: enabling may raise spurious pointer update events
  prv_event_clear(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_STOPPED_OFFSET);
  prv_putreg(i2s, NRF52_I2S_INTENSET_OFFSET, inten);
  prv_putreg(i2s, NRF52_I2S_TASKS_START_OFFSET, 1U);
}

static bool prv_stopped(const struct pbl_audio_nrf5_i2s *i2s) {
  return prv_getreg(i2s, NRF52_I2S_EVENTS_STOPPED_OFFSET) != 0U;
}

// Stop the hardware, leaving the clock and the buffers. Reports whether it had taken the playback
// block queued last, which it then sent in part.
static int prv_hw_halt(const struct pbl_audio_nrf5_i2s *i2s, bool *taken) {
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  int ret;

  // Mask the pointer updates first, which may be spurious once stopping (nRF52 anomaly 55)
  prv_putreg(i2s, NRF52_I2S_INTENCLR_OFFSET, I2S_INT_TXPTRUPD | I2S_INT_RXPTRUPD);
  *taken = prv_getreg(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET) != 0U;
  prv_putreg(i2s, NRF52_I2S_TASKS_STOP_OFFSET, 1U);
  prv_putreg(i2s, ANOMALY_194_OFFSET0, 1U);
  prv_putreg(i2s, ANOMALY_194_OFFSET1, 1U);

  ret = PBL_POLL_BUSY_US(prv_stopped(i2s), 10U, STOP_BUSY_US);
  if (ret != 0) {
    ret = PBL_POLL_SLEEP_MS(prv_stopped(i2s), 1U, STOP_TIMEOUT_MS);
  }

  prv_putreg(i2s, NRF52_I2S_ENABLE_OFFSET, 0U);
  // nRF52 anomaly 196: disabling does not release the pins
  prv_putreg(i2s, NRF52_I2S_PSEL_MCK_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_SCK_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_LRCK_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_SDIN_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_putreg(i2s, NRF52_I2S_PSEL_SDOUT_OFFSET, I2S_PSEL_DISCONNECTED);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  prv_event_clear(i2s, NRF52_I2S_EVENTS_STOPPED_OFFSET);
  pbl_irq_clear_pending(i2s->irq);
  state->running = 0U;

  return ret;
}

// Run the hardware again with dirs, after a direction joined or left: the ones that continue
// lose the rest of the block in flight
static int prv_hw_restart(const struct pbl_audio_nrf5_i2s *i2s, uint8_t dirs, uint8_t continuing) {
  bool taken;
  int ret;

  ret = prv_hw_halt(i2s, &taken);

  for (uint8_t d = 0U; d < PBL_AUDIO_DIRS; d++) {
    if ((continuing & PBL_BIT(d)) != 0U) {
      pbl_audio_report_xrun(prv_card(i2s), d);
    }
  }

  prv_hw_run(i2s, dirs, (continuing & PBL_BIT(PBL_AUDIO_PLAYBACK)) != 0U && !taken);

  return ret;
}

// Start for the first stream, whose periods set the block size. A second joins by restarting
// the hardware with both, so the capture path, its buffers and its interrupts run only while
// something records.
static int prv_start(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir,
                     const struct pbl_audio_stream *stream, int32_t gain_mdb, bool mute) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(dai);
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  const uint8_t running = state->running;
  int ret;

  if (running != 0U) {
    ret = prv_alloc(&state->dirs[dir], state->block_frames);
    if (ret != 0) {
      return ret;
    }

    // Disabled even if the stop timed out, so it runs with both
    (void)prv_hw_restart(i2s, running | PBL_BIT(dir), running);
    return 0;
  }

  if (stream->period_frames > PBL_AUDIO_NRF5_I2S_BOUNCE_FRAMES) {
    return -EINVAL;
  }

  ret = prv_alloc(&state->dirs[dir], stream->period_frames);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&i2s->hfclk);
  if (ret != 0) {
    prv_free(&state->dirs[dir]);
    return ret;
  }

  ret = prv_clocks_configure(i2s, false);
  if (ret != 0) {
    (void)pbl_clk_disable(&i2s->hfclk);
    prv_free(&state->dirs[dir]);
    return ret;
  }

  state->block_frames = stream->period_frames;
  prv_hw_run(i2s, PBL_BIT(dir), false);

  return 0;
}

static int prv_stop(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir) {
  const struct pbl_audio_nrf5_i2s *i2s = prv_dev(dai);
  struct pbl_audio_nrf5_i2s_state *state = i2s->state;
  const uint8_t left = state->running & ~PBL_BIT(dir);
  bool taken;
  int ret;

  if ((state->running & PBL_BIT(dir)) == 0U) {
    return 0;
  }

  if (left != 0U) {
    ret = prv_hw_restart(i2s, left, left);
    prv_free(&state->dirs[dir]);
    return ret;
  }

  ret = prv_hw_halt(i2s, &taken);
  prv_free(&state->dirs[dir]);
  (void)pbl_clk_disable(&i2s->hfclk);
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
