/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/audio/qemu.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <kernel/pbl_malloc.h>

#define RATE     16000U
#define MS_PER_S 1000U
// Host audio queued ahead of the client, enough to absorb host pacing jitter
#define RING_SAMPLES (RATE / 4U)

static const struct pbl_audio_host_mic *s_running;

static const struct pbl_audio_host_mic *prv_dev(const struct pbl_audio *audio) {
  return container_of(audio, const struct pbl_audio_host_mic, audio);
}

static void prv_fill(struct pbl_audio_host_mic_state *state, int16_t *out, size_t count) {
  size_t filled = 0U;

  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  while (state->ring != nullptr && filled < count && state->ring_count > 0U) {
    size_t run = MIN(count - filled, MIN(state->ring_count, RING_SAMPLES - state->ring_read));

    memcpy(&out[filled], &state->ring[state->ring_read], run * sizeof(int16_t));
    filled += run;
    state->ring_read = (state->ring_read + run) % RING_SAMPLES;
    state->ring_count -= run;
  }
  pbl_mutex_unlock(&state->lock);

  memset(&out[filled], 0, (count - filled) * sizeof(int16_t));
}

static void prv_timer_cb(void *data) {
  const struct pbl_audio_host_mic *mic = data;
  const struct pbl_audio_state *audio_state = mic->audio.state;
  struct pbl_audio_host_mic_state *state = mic->state;

  if (!pbl_audio_is_running(&mic->audio)) {
    return;
  }

  prv_fill(state, pbl_audio_period(&mic->audio, state->idx), audio_state->stream.period_frames);
  state->idx = (state->idx + 1U) % audio_state->stream.periods;
  pbl_audio_period_elapsed(&mic->audio);
}

void qemu_mic_msg_callback(const uint8_t *data, uint32_t len) {
  const struct pbl_audio_host_mic *mic = s_running;
  struct pbl_audio_host_mic_state *state;
  size_t count = len / sizeof(int16_t);
  size_t overflow;
  size_t write;

  if (mic == nullptr) {
    return;
  }

  state = mic->state;
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  if (state->ring == nullptr) {
    pbl_mutex_unlock(&state->lock);
    return;
  }

  // Keep the newest audio: skip what cannot fit, then drop the oldest to make room
  if (count > RING_SAMPLES) {
    data += (count - RING_SAMPLES) * sizeof(int16_t);
    count = RING_SAMPLES;
  }
  overflow =
      state->ring_count + count > RING_SAMPLES ? state->ring_count + count - RING_SAMPLES : 0U;
  state->ring_read = (state->ring_read + overflow) % RING_SAMPLES;
  state->ring_count -= overflow;

  write = (state->ring_read + state->ring_count) % RING_SAMPLES;
  while (count > 0U) {
    size_t run = MIN(count, RING_SAMPLES - write);

    memcpy(&state->ring[write], data, run * sizeof(int16_t));
    data += run * sizeof(int16_t);
    count -= run;
    state->ring_count += run;
    write = (write + run) % RING_SAMPLES;
  }
  pbl_mutex_unlock(&state->lock);
}

static int prv_init(const struct pbl_audio *audio) {
  struct pbl_audio_host_mic_state *state = prv_dev(audio)->state;

  *state = (struct pbl_audio_host_mic_state){0};
  pbl_mutex_init(&state->lock);
  state->timer = new_timer_create();

  return state->timer != TIMER_INVALID_ID ? 0 : -ENOMEM;
}

static void prv_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 1U;
}

static void prv_free_ring(struct pbl_audio_host_mic_state *state) {
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  kernel_free(state->ring);
  state->ring = nullptr;
  state->ring_count = 0U;
  pbl_mutex_unlock(&state->lock);
}

static int prv_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_host_mic *mic = prv_dev(audio);
  struct pbl_audio_host_mic_state *state = mic->state;
  uint32_t period_ms = DIVIDE_CEIL(stream->period_frames * MS_PER_S, stream->rate);

  // Without the ring the host audio is dropped and the client records silence
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  state->ring = kernel_malloc(RING_SAMPLES * sizeof(int16_t));
  state->ring_read = 0U;
  state->ring_count = 0U;
  state->idx = 0U;
  pbl_mutex_unlock(&state->lock);

  if (!new_timer_start(state->timer, period_ms, prv_timer_cb, (void *)mic,
                       TIMER_START_FLAG_REPEATING)) {
    prv_free_ring(state);
    return -EIO;
  }

  s_running = mic;

  return 0;
}

static int prv_stop(const struct pbl_audio *audio) {
  struct pbl_audio_host_mic_state *state = prv_dev(audio)->state;

  s_running = nullptr;
  new_timer_stop(state->timer);
  prv_free_ring(state);

  return 0;
}

const struct pbl_audio_ops pbl_audio_host_mic_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
};
