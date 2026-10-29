/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_audio.h"

#include <errno.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/battery.h>
#include <pbl/kernel/irq.h>
#include <pbl/services/system_task.h>
#include <pbl/util/circular_buffer.h>
#include <pbl/util/math.h>

#include <kernel/pbl_malloc.h>

#define MS_PER_S      1000
#define PERIOD_MS     20
#define PERIOD_FRAMES (MFG_AUDIO_SAMPLE_RATE * PERIOD_MS / MS_PER_S)
#define PERIODS       2
#define MAX_CHANNELS  2
#define CHUNK_SAMPLES (PERIOD_FRAMES * MAX_CHANNELS)
// Queue between the device interrupt and KernelBG
#define FIFO_PERIODS 8

struct prv_session {
  const struct pbl_audio *audio;
  bool running;
  volatile bool pending;
  uint8_t channels;
  int16_t *ring;
  uint8_t *fifo_storage;
  CircularBuffer fifo;
  int16_t chunk[CHUNK_SAMPLES];
  MfgAudioCaptureCb capture_cb;
  MfgAudioFillCb fill_cb;
};

static size_t prv_period_bytes(const struct prv_session *s) {
  return PERIOD_FRAMES * s->channels * sizeof(int16_t);
}

static void prv_bottom_half(void *data) {
  struct prv_session *s = data;
  const size_t bytes = prv_period_bytes(s);

  s->pending = false;
  while (s->running) {
    pbl_irq_lock();
    const bool ready = s->capture_cb != nullptr
                           ? circular_buffer_get_read_space_remaining(&s->fifo) >= bytes
                           : circular_buffer_get_write_space_remaining(&s->fifo) >= bytes;
    if (ready && s->capture_cb != nullptr) {
      (void)circular_buffer_copy(&s->fifo, s->chunk, bytes);
      circular_buffer_consume(&s->fifo, bytes);
    }
    pbl_irq_unlock();

    if (!ready) {
      break;
    }

    if (s->capture_cb != nullptr) {
      s->capture_cb(s->chunk, bytes / sizeof(int16_t));
      continue;
    }

    const size_t count = s->fill_cb(s->chunk, bytes / sizeof(int16_t));
    if (count == 0 || !s->running) {
      break;
    }
    pbl_irq_lock();
    (void)circular_buffer_write(&s->fifo, s->chunk, count * sizeof(int16_t));
    pbl_irq_unlock();
  }
}

static void prv_period_done(const struct pbl_audio *audio, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  struct prv_session *s = arg;
  const size_t bytes = prv_period_bytes(s);

  if (audio->dir == PBL_AUDIO_CAPTURE) {
    (void)circular_buffer_write(&s->fifo, period, bytes);
  } else {
    const uint16_t avail = MIN(circular_buffer_get_read_space_remaining(&s->fifo), bytes);
    (void)circular_buffer_copy(&s->fifo, period, avail);
    circular_buffer_consume(&s->fifo, avail);
  }
  (void)pbl_audio_period_ready(audio, period);

  if (!s->pending) {
    s->pending = true;
    if (!system_task_add_callback_from_isr_droppable(prv_bottom_half, s)) {
      s->pending = false;
    }
  }
}

static void prv_free(struct prv_session *s) {
  pbl_audio_buf_free(s->ring);
  kernel_free(s->fifo_storage);
  kernel_free(s);
}

static void prv_free_cb(void *data) {
  prv_free(data);
}

static struct prv_session *prv_start(const struct pbl_audio *audio, uint8_t channels,
                                     int32_t gain_mdb, MfgAudioCaptureCb capture_cb,
                                     MfgAudioFillCb fill_cb) {
  struct prv_session *s = kernel_zalloc(sizeof(*s));
  struct pbl_audio_stream stream = {
    .rate = MFG_AUDIO_SAMPLE_RATE,
    .channels = channels,
    .period_frames = PERIOD_FRAMES,
    .periods = PERIODS,
    .period_done = prv_period_done,
  };

  if (s == nullptr || channels > MAX_CHANNELS) {
    kernel_free(s);
    return nullptr;
  }

  s->audio = audio;
  s->channels = channels;
  s->capture_cb = capture_cb;
  s->fill_cb = fill_cb;
  s->ring = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
  s->fifo_storage = kernel_malloc(FIFO_PERIODS * prv_period_bytes(s));
  if (s->ring == nullptr || s->fifo_storage == nullptr) {
    prv_free(s);
    return nullptr;
  }

  circular_buffer_init(&s->fifo, s->fifo_storage, FIFO_PERIODS * prv_period_bytes(s));
  stream.buf = s->ring;
  stream.arg = s;
  s->running = true;

  if (pbl_audio_set_gain(audio, gain_mdb) != 0 || pbl_audio_set_mute(audio, false) != 0 ||
      pbl_audio_start(audio, &stream) != 0) {
    prv_free(s);
    return nullptr;
  }

  return s;
}

static void prv_stop(struct prv_session *s) {
  s->running = false;
  (void)pbl_audio_stop(s->audio);
  // A queued bottom half may still reference the session
  (void)system_task_add_callback(prv_free_cb, s);
}

#ifdef CONFIG_MIC
static struct prv_session *s_capture;

int mfg_audio_capture_start(uint8_t channels, int32_t gain_mdb, MfgAudioCaptureCb cb) {
  if (s_capture != nullptr) {
    return -EBUSY;
  }

  s_capture = prv_start(MIC, channels, gain_mdb, cb, nullptr);
  if (s_capture == nullptr) {
    return -EIO;
  }

  return 0;
}

void mfg_audio_capture_stop(void) {
  if (s_capture != nullptr) {
    prv_stop(s_capture);
    s_capture = nullptr;
  }
}
#endif

#ifdef CONFIG_SPEAKER
static struct prv_session *s_playback;

//! The speaker draws more than the default battery discharge limit allows
static void prv_high_load(bool on) {
#ifdef CONFIG_BATTERY_HIGH_LOAD
  if (on) {
    (void)battery_high_load_request();
  } else {
    (void)battery_high_load_release();
  }
#endif
}

int mfg_audio_playback_start(int32_t gain_mdb, MfgAudioFillCb cb) {
  if (s_playback != nullptr) {
    return -EBUSY;
  }

  prv_high_load(true);
  s_playback = prv_start(SPEAKER, 1, gain_mdb, nullptr, cb);
  if (s_playback == nullptr) {
    prv_high_load(false);
    return -EIO;
  }

  return 0;
}

void mfg_audio_playback_stop(void) {
  if (s_playback != nullptr) {
    prv_stop(s_playback);
    s_playback = nullptr;
    prv_high_load(false);
  }
}
#endif
