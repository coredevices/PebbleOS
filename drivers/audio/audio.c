/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/audio.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DEFINE(driver_audio, CONFIG_DRIVER_AUDIO_LOG_LEVEL);

PBL_DEVICE_CLASS_DEFINE(pbl_audio_class, "audio");

#define US_PER_S 1000000U

static const uint32_t s_rates[] = {
  8000U, 11025U, 12000U, 16000U, 22050U, 24000U, 32000U, 44100U, 48000U,
};

uint32_t pbl_audio_rate_bit(uint32_t rate) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_rates); i++) {
    if (s_rates[i] == rate) {
      return PBL_BIT(i);
    }
  }

  return 0U;
}

static uint64_t prv_now_us(void) {
  return pbl_uptime_ticks() * US_PER_S / PBL_TICK_HZ;
}

static const struct pbl_audio_codec *prv_codec(const struct pbl_audio *audio) {
  return audio->link != nullptr ? audio->link->codec : nullptr;
}

static const struct pbl_audio_amp *prv_amp(const struct pbl_audio *audio) {
  return audio->link != nullptr ? audio->link->amp : nullptr;
}

static void prv_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  const struct pbl_audio_codec *codec = prv_codec(audio);

  *caps = (struct pbl_audio_caps){0};
  audio->ops->get_caps(audio, caps);

  if (codec != nullptr) {
    caps->gain_min_mdb = 0;
    caps->gain_max_mdb = 0;
    caps->gain_step_mdb = 0;
    caps->gain_default_mdb = 0;
    codec->ops->get_caps(codec, caps);
  }

  if (caps->max_periods == 0U || caps->max_periods > PBL_AUDIO_MAX_PERIODS) {
    caps->max_periods = PBL_AUDIO_MAX_PERIODS;
  }
}

int pbl_audio_dev_init(const struct pbl_device *dev) {
  const struct pbl_audio *audio = container_of(dev, const struct pbl_audio, dev);
  const struct pbl_audio_codec *codec = prv_codec(audio);
  const struct pbl_audio_amp *amp = prv_amp(audio);
  struct pbl_audio_state *state = audio->state;
  struct pbl_audio_caps caps;
  int ret;

  *state = (struct pbl_audio_state){0};
  pbl_mutex_init(&state->lock);

  if (codec != nullptr) {
    ret = pbl_device_init(&codec->dev);
    if (ret != 0) {
      return ret;
    }
  }

  if (amp != nullptr) {
    ret = pbl_device_init(&amp->dev);
    if (ret != 0) {
      return ret;
    }
  }

  if (audio->ops->init != nullptr) {
    ret = audio->ops->init(audio);
    if (ret != 0) {
      return ret;
    }
  }

  prv_get_caps(audio, &caps);
  state->gain_mdb = caps.gain_default_mdb;
  state->gain_min_mdb = caps.gain_min_mdb;

  return 0;
}

static void prv_lock(const struct pbl_audio *audio) {
  pbl_mutex_lock(&audio->state->lock, PBL_FOREVER);
}

static void prv_unlock(const struct pbl_audio *audio) {
  pbl_mutex_unlock(&audio->state->lock);
}

int pbl_audio_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  if (!pbl_device_is_ready(&audio->dev)) {
    return -ENODEV;
  }

  prv_get_caps(audio, caps);

  return 0;
}

size_t pbl_audio_ring_bytes(const struct pbl_audio_stream *stream) {
  size_t bytes = stream->period_frames * stream->channels * sizeof(int16_t) * stream->periods;

  return DIVIDE_CEIL(bytes, PBL_AUDIO_BUF_ALIGN) * PBL_AUDIO_BUF_ALIGN;
}

void *pbl_audio_buf_alloc(size_t size) {
  uint8_t *raw = kernel_malloc(size + PBL_AUDIO_BUF_ALIGN + sizeof(void *));
  uintptr_t aligned;

  if (raw == nullptr) {
    return nullptr;
  }

  aligned = DIVIDE_CEIL((uintptr_t)raw + sizeof(void *), PBL_AUDIO_BUF_ALIGN) * PBL_AUDIO_BUF_ALIGN;
  ((void **)aligned)[-1] = raw;
  memset((void *)aligned, 0, size);

  return (void *)aligned;
}

void pbl_audio_buf_free(void *buf) {
  if (buf != nullptr) {
    kernel_free(((void **)buf)[-1]);
  }
}

static int prv_check_stream(const struct pbl_audio *audio, const struct pbl_audio_stream *stream) {
  struct pbl_audio_caps caps;

  prv_get_caps(audio, &caps);

  if ((pbl_audio_rate_bit(stream->rate) & caps.rates) == 0U ||
      stream->channels < caps.min_channels || stream->channels > caps.max_channels ||
      stream->periods < 2U || stream->periods > caps.max_periods || stream->period_frames == 0U ||
      stream->buf == nullptr || stream->period_done == nullptr ||
      ((uintptr_t)stream->buf % PBL_AUDIO_BUF_ALIGN) != 0U) {
    return -EINVAL;
  }

  return 0;
}

static int32_t prv_clamp_gain(const struct pbl_audio_caps *caps, int32_t gain_mdb) {
  int32_t gain = CLIP(gain_mdb, caps->gain_min_mdb, caps->gain_max_mdb);

  if (caps->gain_step_mdb > 0) {
    gain =
        caps->gain_min_mdb +
        pbl_audio_gain_to_step(gain, caps->gain_min_mdb, caps->gain_step_mdb) * caps->gain_step_mdb;
  }

  return gain;
}

static int prv_stream_up(const struct pbl_audio *audio, const struct pbl_audio_stream *stream) {
  const struct pbl_audio_codec *codec = prv_codec(audio);
  struct pbl_audio_state *state = audio->state;
  int ret;

  if (codec == nullptr) {
    return audio->ops->start(audio, stream, state->gain_mdb, state->mute);
  }

  ret = audio->ops->start(audio, stream, 0, false);
  if (ret != 0) {
    return ret;
  }

  const struct pbl_audio_codec_cfg cfg = {
    .rate = stream->rate,
    .channels = stream->channels,
    .fmt = audio->link->fmt,
    .gain_mdb = state->gain_mdb,
    .mute = state->mute,
  };

  ret = codec->ops->enable(codec, &cfg);
  if (ret != 0) {
    (void)audio->ops->stop(audio);
  }

  return ret;
}

static int prv_power_up(const struct pbl_audio *audio, const struct pbl_audio_stream *stream) {
  const struct pbl_audio_amp *amp = prv_amp(audio);
  int ret;

  if (amp == nullptr) {
    return prv_stream_up(audio, stream);
  }

  ret = amp->ops->enable(amp);
  if (ret != 0) {
    return ret;
  }

  ret = prv_stream_up(audio, stream);
  if (ret != 0) {
    (void)amp->ops->disable(amp);
  }

  return ret;
}

static void prv_set_running(struct pbl_audio_state *state, bool running) {
  pbl_irq_lock();
  state->running = running;
  pbl_irq_unlock();
}

static int prv_start_locked(const struct pbl_audio *audio, const struct pbl_audio_stream *stream) {
  struct pbl_audio_state *state = audio->state;
  int ret;

  if (state->running) {
    return -EBUSY;
  }

  ret = prv_check_stream(audio, stream);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_lock();
  state->stream = *stream;
  state->period_bytes = stream->period_frames * stream->channels * sizeof(int16_t);
  state->hw = 0U;
  state->owned = 0U;
  state->frame = 0U;
  state->xrun = false;
  state->ts_us = prv_now_us();
  state->running = true;
  pbl_irq_unlock();

  ret = prv_power_up(audio, stream);
  if (ret != 0) {
    prv_set_running(state, false);
  }

  return ret;
}

int pbl_audio_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream) {
  int ret;

  if (!pbl_device_is_ready(&audio->dev)) {
    return -ENODEV;
  }

  prv_lock(audio);
  ret = prv_start_locked(audio, stream);
  prv_unlock(audio);

  if (ret != 0) {
    PBL_LOG_DBG("Start of %s failed (%d)", audio->dev.name, ret);
  }

  return ret;
}

static int prv_stop_locked(const struct pbl_audio *audio) {
  const struct pbl_audio_codec *codec = prv_codec(audio);
  const struct pbl_audio_amp *amp = prv_amp(audio);
  struct pbl_audio_state *state = audio->state;
  int ret = 0;
  int err;

  if (!state->running) {
    return 0;
  }

  if (codec != nullptr) {
    ret = codec->ops->disable(codec);
  }

  prv_set_running(state, false);

  err = audio->ops->stop(audio);
  if (ret == 0) {
    ret = err;
  }

  if (amp != nullptr) {
    err = amp->ops->disable(amp);
    if (ret == 0) {
      ret = err;
    }
  }

  return ret;
}

int pbl_audio_stop(const struct pbl_audio *audio) {
  int ret;

  if (!pbl_device_is_ready(&audio->dev)) {
    return -ENODEV;
  }

  prv_lock(audio);
  ret = prv_stop_locked(audio);
  prv_unlock(audio);

  return ret;
}

void *pbl_audio_period(const struct pbl_audio *audio, uint8_t idx) {
  const struct pbl_audio_state *state = audio->state;

  return (uint8_t *)state->stream.buf + (size_t)idx * state->period_bytes;
}

void pbl_audio_period_elapsed(const struct pbl_audio *audio) {
  struct pbl_audio_state *state = audio->state;
  const struct pbl_audio_stream *stream = &state->stream;
  struct pbl_audio_pos pos;
  uint8_t done;
  uint8_t next;
  void *period;

  pbl_irq_lock();
  if (!state->running) {
    pbl_irq_unlock();
    return;
  }

  done = state->hw;
  next = (done + 1U) % stream->periods;
  state->hw = next;
  state->frame += stream->period_frames;
  state->ts_us = prv_now_us();

  pos.frame = state->frame;
  pos.ts_us = state->ts_us;
  pos.xrun = state->xrun || (state->owned & PBL_BIT(next)) != 0U ||
             (audio->dir == PBL_AUDIO_CAPTURE && (state->owned & PBL_BIT(done)) != 0U);
  state->xrun = false;
  state->owned |= PBL_BIT(done);
  pbl_irq_unlock();

  period = pbl_audio_period(audio, done);
  if (audio->dir == PBL_AUDIO_PLAYBACK) {
    memset(period, 0, state->period_bytes);
    if (audio->ops->sync_for_device != nullptr) {
      audio->ops->sync_for_device(audio, period, state->period_bytes);
    }
  } else if (audio->ops->sync_for_cpu != nullptr) {
    audio->ops->sync_for_cpu(audio, period, state->period_bytes);
  }

  stream->period_done(audio, period, &pos, stream->arg);
}

void pbl_audio_report_xrun(const struct pbl_audio *audio) {
  pbl_irq_lock();
  audio->state->xrun = true;
  pbl_irq_unlock();
}

int pbl_audio_period_ready(const struct pbl_audio *audio, void *period) {
  struct pbl_audio_state *state = audio->state;
  uintptr_t offset;
  uint32_t bit;

  pbl_irq_lock();
  if (!state->running) {
    pbl_irq_unlock();
    return -EAGAIN;
  }

  offset = (uintptr_t)period - (uintptr_t)state->stream.buf;
  if ((uintptr_t)period < (uintptr_t)state->stream.buf || (offset % state->period_bytes) != 0U ||
      offset / state->period_bytes >= state->stream.periods) {
    pbl_irq_unlock();
    return -EINVAL;
  }

  bit = PBL_BIT(offset / state->period_bytes);
  if ((state->owned & bit) == 0U) {
    pbl_irq_unlock();
    return -EINVAL;
  }

  state->owned &= ~bit;
  pbl_irq_unlock();

  if (audio->dir == PBL_AUDIO_PLAYBACK && audio->ops->sync_for_device != nullptr) {
    audio->ops->sync_for_device(audio, period, state->period_bytes);
  }

  return 0;
}

static int prv_apply_gain_locked(const struct pbl_audio *audio) {
  const struct pbl_audio_codec *codec = prv_codec(audio);
  struct pbl_audio_state *state = audio->state;

  if (!state->running) {
    return 0;
  }

  if (codec != nullptr) {
    return codec->ops->set_gain(codec, state->gain_mdb);
  }

  if (audio->ops->set_gain == nullptr) {
    return 0;
  }

  if (state->mute && audio->ops->set_mute == nullptr) {
    return audio->ops->set_gain(audio, state->gain_min_mdb);
  }

  return audio->ops->set_gain(audio, state->gain_mdb);
}

int pbl_audio_set_gain(const struct pbl_audio *audio, int32_t gain_mdb) {
  struct pbl_audio_caps caps;
  int ret;

  if (!pbl_device_is_ready(&audio->dev)) {
    return -ENODEV;
  }

  prv_get_caps(audio, &caps);

  prv_lock(audio);
  audio->state->gain_mdb = prv_clamp_gain(&caps, gain_mdb);
  ret = prv_apply_gain_locked(audio);
  prv_unlock(audio);

  return ret;
}

static int prv_apply_mute_locked(const struct pbl_audio *audio) {
  const struct pbl_audio_codec *codec = prv_codec(audio);
  struct pbl_audio_state *state = audio->state;

  if (!state->running) {
    return 0;
  }

  if (codec != nullptr) {
    return codec->ops->set_mute(codec, state->mute);
  }

  if (audio->ops->set_mute == nullptr) {
    return prv_apply_gain_locked(audio);
  }

  return audio->ops->set_mute(audio, state->mute);
}

int pbl_audio_set_mute(const struct pbl_audio *audio, bool mute) {
  int ret;

  if (!pbl_device_is_ready(&audio->dev)) {
    return -ENODEV;
  }

  prv_lock(audio);
  audio->state->mute = mute;
  ret = prv_apply_mute_locked(audio);
  prv_unlock(audio);

  return ret;
}

int pbl_audio_get_position(const struct pbl_audio *audio, struct pbl_audio_pos *pos) {
  struct pbl_audio_state *state = audio->state;
  size_t frames = 0U;

  pbl_irq_lock();
  if (!state->running) {
    pbl_irq_unlock();
    return -EAGAIN;
  }

  pos->frame = state->frame;
  pos->ts_us = state->ts_us;
  pos->xrun = false;

  if (audio->ops->pointer != nullptr && audio->ops->pointer(audio, &frames) == 0) {
    pos->frame += MIN(frames, state->stream.period_frames);
    pos->ts_us = prv_now_us();
  }
  pbl_irq_unlock();

  return 0;
}

bool pbl_audio_is_running(const struct pbl_audio *audio) {
  return audio->state->running;
}
