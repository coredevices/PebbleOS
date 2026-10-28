/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/audio/qemu.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

// Registers of the pebble-audio QEMU device
#define AUDIO_CTRL       0x00
#define AUDIO_SAMPLERATE 0x08
#define AUDIO_DATA       0x0c
#define AUDIO_INTCTRL    0x10
#define AUDIO_INTSTAT    0x14
#define AUDIO_BUFAVAIL   0x18
#define AUDIO_VOLUME     0x1c

#define CTRL_ENABLE  PBL_BIT(0)
#define INT_BUFAVAIL PBL_BIT(0)

// The device takes a volume in percent; map it linearly from the gain range
#define GAIN_MIN_MDB    (-36000)
#define VOLUME_MAX      100
#define MDB_PER_PERCENT (-GAIN_MIN_MDB / VOLUME_MAX)

static const struct pbl_audio_qemu *prv_dev(const struct pbl_audio *audio) {
  return container_of(audio, const struct pbl_audio_qemu, audio);
}

static void prv_write(const struct pbl_audio_qemu *dev, uint32_t reg, uint32_t value) {
  pbl_writel(value, dev->base + reg);
}

static uint32_t prv_read(const struct pbl_audio_qemu *dev, uint32_t reg) {
  return pbl_readl(dev->base + reg);
}

static uint32_t prv_volume(int32_t gain_mdb, bool mute) {
  return mute ? 0U : (uint32_t)(VOLUME_MAX + gain_mdb / MDB_PER_PERCENT);
}

static void prv_push(const struct pbl_audio_qemu *dev) {
  const struct pbl_audio_state *audio_state = dev->audio.state;
  struct pbl_audio_qemu_state *state = dev->state;
  size_t period_samples = audio_state->stream.period_frames;
  uint32_t avail = prv_read(dev, AUDIO_BUFAVAIL);

  while (avail > 0U && pbl_audio_is_running(&dev->audio)) {
    const int16_t *period = pbl_audio_period(&dev->audio, state->idx);

    for (; avail > 0U && state->offset < period_samples; avail--, state->offset++) {
      prv_write(dev, AUDIO_DATA, (uint16_t)period[state->offset]);
    }

    if (state->offset == period_samples) {
      state->offset = 0U;
      state->idx = (state->idx + 1U) % audio_state->stream.periods;
      pbl_audio_period_elapsed(&dev->audio);
    }
  }
}

void pbl_audio_qemu_irq_handler(const struct pbl_audio_qemu *dev) {
  prv_write(dev, AUDIO_INTSTAT, INT_BUFAVAIL);
  prv_push(dev);
}

static int prv_init(const struct pbl_audio *audio) {
  prv_write(prv_dev(audio), AUDIO_CTRL, 0U);

  return 0;
}

static void prv_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 1U;
  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = 0;
  caps->gain_step_mdb = MDB_PER_PERCENT;
  caps->gain_default_mdb = 0;
}

static int prv_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_qemu *dev = prv_dev(audio);

  *dev->state = (struct pbl_audio_qemu_state){0};

  prv_write(dev, AUDIO_VOLUME, prv_volume(gain_mdb, mute));
  prv_write(dev, AUDIO_SAMPLERATE, stream->rate);
  prv_write(dev, AUDIO_INTSTAT, INT_BUFAVAIL);
  prv_write(dev, AUDIO_INTCTRL, INT_BUFAVAIL);
  prv_write(dev, AUDIO_CTRL, CTRL_ENABLE);

  pbl_irq_lock();
  prv_push(dev);
  pbl_irq_unlock();

  pbl_irq_enable(dev->irq);

  return 0;
}

static int prv_stop(const struct pbl_audio *audio) {
  const struct pbl_audio_qemu *dev = prv_dev(audio);

  prv_write(dev, AUDIO_CTRL, 0U);
  prv_write(dev, AUDIO_INTCTRL, 0U);
  pbl_irq_disable(dev->irq);

  return 0;
}

static int prv_set_gain(const struct pbl_audio *audio, int32_t gain_mdb) {
  prv_write(prv_dev(audio), AUDIO_VOLUME, prv_volume(gain_mdb, audio->state->mute));

  return 0;
}

static int prv_set_mute(const struct pbl_audio *audio, bool mute) {
  prv_write(prv_dev(audio), AUDIO_VOLUME, prv_volume(audio->state->gain_mdb, mute));

  return 0;
}

const struct pbl_audio_ops pbl_audio_qemu_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .set_gain = prv_set_gain,
  .set_mute = prv_set_mute,
};
