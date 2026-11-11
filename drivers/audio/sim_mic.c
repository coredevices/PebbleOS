/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/audio/sim.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#define RATE     16000U
#define MS_PER_S 1000U
// Audio queued ahead of the client, enough to absorb the writer pacing jitter
#define RING_SAMPLES (RATE / 4U)

struct pbl_audio_sim_mic_state {
  const struct pbl_audio_sim_mic *mic;
  struct pbl_delayed_work timer;
  pbl_tick_t period_ticks;
  bool repeat;
  struct pbl_mutex lock;
  int16_t *ring;
  size_t ring_read;
  size_t ring_count;
  uint8_t idx;
};

struct pbl_audio_sim_mic {
  struct pbl_audio_dai dai;
  struct pbl_audio_sim_mic_state *state;
};

static const struct pbl_audio_sim_mic *prv_dev(const struct pbl_audio_dai *dai) {
  return container_of(dai, const struct pbl_audio_sim_mic, dai);
}

static void prv_fill(struct pbl_audio_sim_mic_state *state, int16_t *out, size_t count) {
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

static void prv_timer_fn(struct pbl_work *work) {
  struct pbl_audio_sim_mic_state *state =
      container_of(work, struct pbl_audio_sim_mic_state, timer.work);
  const struct pbl_audio_sim_mic *mic = state->mic;
  const struct pbl_audio_state *audio_state = pbl_audio_dai_card(&mic->dai)->state;

  if (pbl_audio_is_running(pbl_audio_dai_card(&mic->dai))) {
    prv_fill(state, pbl_audio_period(pbl_audio_dai_card(&mic->dai), state->idx),
             audio_state->stream.period_frames);
    state->idx = (state->idx + 1U) % audio_state->stream.periods;
    pbl_audio_period_elapsed(pbl_audio_dai_card(&mic->dai));
  }

  // prv_stop() can land while this runs
  pbl_irq_lock();
  if (state->repeat && !pbl_delayed_work_is_pending(&state->timer)) {
    pbl_delayed_work_schedule_next(&state->timer, state->period_ticks);
  }
  pbl_irq_unlock();
}

void pbl_audio_sim_mic_write(const struct pbl_audio *mic, const void *pcm, size_t count) {
  struct pbl_audio_sim_mic_state *state = prv_dev(mic->dai)->state;
  const uint8_t *data = pcm;
  size_t overflow;
  size_t write;

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

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_sim_mic *mic = prv_dev(dai);
  struct pbl_audio_sim_mic_state *state = mic->state;

  *state = (struct pbl_audio_sim_mic_state){.mic = mic};
  pbl_mutex_init(&state->lock);
  pbl_delayed_work_init(&state->timer, prv_timer_fn);

  return 0;
}

static void prv_get_caps(const struct pbl_audio_dai *dai, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 1U;
}

static void prv_free_ring(struct pbl_audio_sim_mic_state *state) {
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  pbl_free(state->ring);
  state->ring = nullptr;
  state->ring_count = 0U;
  pbl_mutex_unlock(&state->lock);
}

static int prv_start(const struct pbl_audio_dai *dai, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_sim_mic *mic = prv_dev(dai);
  struct pbl_audio_sim_mic_state *state = mic->state;
  uint32_t period_ms = DIVIDE_CEIL(stream->period_frames * MS_PER_S, stream->rate);

  // Without the ring the host audio is dropped and the client records silence
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  state->ring = pbl_malloc(RING_SAMPLES * sizeof(int16_t));
  state->ring_read = 0U;
  state->ring_count = 0U;
  state->idx = 0U;
  pbl_mutex_unlock(&state->lock);

  state->period_ticks = pbl_ms_to_ticks(period_ms);
  state->repeat = true;
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &state->timer, PBL_MSEC(period_ms));

  return 0;
}

static int prv_stop(const struct pbl_audio_dai *dai) {
  struct pbl_audio_sim_mic_state *state = prv_dev(dai)->state;

  pbl_irq_lock();
  state->repeat = false;
  pbl_delayed_work_cancel(&state->timer);
  pbl_irq_unlock();
  prv_free_ring(state);

  return 0;
}

static const struct pbl_audio_dai_ops s_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
};

PBL_AUDIO_DAI_STATE_DEFINE(s_sim_mic);
static struct pbl_audio_sim_mic_state s_sim_mic_state;
static const struct pbl_audio_sim_mic s_sim_mic = {
  .dai = PBL_AUDIO_DAI_INIT(s_sim_mic, "sim_mic", PBL_AUDIO_CAPTURE, &s_ops, nullptr),
  .state = &s_sim_mic_state,
};
PBL_DEVICE_REGISTER(s_sim_mic, &s_sim_mic.dai.dev);
PBL_AUDIO_CARD_DEFINE(pbl_audio_sim_mic, "mic", &s_sim_mic.dai, nullptr, nullptr, 0, 0);
