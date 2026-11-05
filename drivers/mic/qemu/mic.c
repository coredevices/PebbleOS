/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/mic.h>
#include <pbl/drivers/mic/qemu/mic_definitions.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/util/math.h>

#include <board/board.h>

PBL_LOG_MODULE_DEFINE(driver_mic_qemu, CONFIG_DRIVER_MIC_LOG_LEVEL);

// Host audio queued ahead of the consumer, enough to absorb host pacing jitter.
#define RING_SAMPLES (MIC_SAMPLE_RATE / 4)

// Fill the consumer's buffer from the host audio ring, padding with silence when the host has
// sent nothing (QEMU has no real microphone) or fallen behind.
static void prv_fill_buffer(MicDeviceState *state) {
  size_t filled = 0;
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  if (state->ring) {
    while (filled < state->audio_buffer_len && state->ring_count) {
      size_t run = MIN(state->audio_buffer_len - filled,
                       MIN(state->ring_count, RING_SAMPLES - state->ring_read));
      memcpy(&state->audio_buffer[filled], &state->ring[state->ring_read], run * sizeof(int16_t));
      filled += run;
      state->ring_read = (state->ring_read + run) % RING_SAMPLES;
      state->ring_count -= run;
    }
  }
  pbl_mutex_unlock(&state->lock);
  memset(&state->audio_buffer[filled], 0, (state->audio_buffer_len - filled) * sizeof(int16_t));
}

static void prv_timer_cb(void *data) {
  MicDevice *this = data;
  MicDeviceState *state = this->state;

  if (!state->is_running || !state->data_handler || !state->audio_buffer) {
    return;
  }

  prv_fill_buffer(state);
  state->data_handler(state->audio_buffer, state->audio_buffer_len, state->handler_context);
}

void qemu_mic_msg_callback(const uint8_t *data, uint32_t len) {
  MicDeviceState *state = MIC->state;
  size_t count = len / sizeof(int16_t);

  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  if (!state->ring) {
    pbl_mutex_unlock(&state->lock);
    return;
  }
  // Keep the newest audio: skip what can't fit, then make room by dropping the oldest.
  if (count > RING_SAMPLES) {
    data += (count - RING_SAMPLES) * sizeof(int16_t);
    count = RING_SAMPLES;
  }
  size_t overflow =
      state->ring_count + count > RING_SAMPLES ? state->ring_count + count - RING_SAMPLES : 0;
  state->ring_read = (state->ring_read + overflow) % RING_SAMPLES;
  state->ring_count -= overflow;

  size_t write = (state->ring_read + state->ring_count) % RING_SAMPLES;
  while (count) {
    size_t run = MIN(count, RING_SAMPLES - write);
    memcpy(&state->ring[write], data, run * sizeof(int16_t));
    data += run * sizeof(int16_t);
    count -= run;
    state->ring_count += run;
    write = (write + run) % RING_SAMPLES;
  }
  pbl_mutex_unlock(&state->lock);
}

static void prv_free_ring(MicDeviceState *state) {
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  pbl_free(state->ring);
  state->ring = nullptr;
  state->ring_count = 0;
  pbl_mutex_unlock(&state->lock);
}

void mic_init(MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);

  MicDeviceState *state = this->state;
  if (state->is_initialized) {
    return;
  }

  state->timer = new_timer_create();
  PBL_ASSERTN(state->timer != TIMER_INVALID_ID);
  pbl_mutex_init(&state->lock);
  state->is_initialized = true;
}

void mic_set_volume(MicDevice *this, uint16_t volume) {
  // No gain stage to tweak on the QEMU stub.
}

bool mic_start(MicDevice *this, MicDataHandlerCB data_handler, void *context, int16_t *audio_buffer,
               size_t audio_buffer_len) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  PBL_ASSERTN(data_handler);
  PBL_ASSERTN(audio_buffer);
  PBL_ASSERTN(audio_buffer_len > 0);

  MicDeviceState *state = this->state;
  PBL_ASSERTN(state->is_initialized);

  if (state->is_running) {
    return false;
  }

  state->data_handler = data_handler;
  state->handler_context = context;
  state->audio_buffer = audio_buffer;
  state->audio_buffer_len = audio_buffer_len;

  // Match the wall-clock cadence of a real mic: one buffer's worth of samples
  // per period at MIC_SAMPLE_RATE. Round up to keep the period >= 1 ms.
  uint32_t period_ms = (audio_buffer_len * 1000U + MIC_SAMPLE_RATE - 1U) / MIC_SAMPLE_RATE;
  if (period_ms == 0) {
    period_ms = 1;
  }
  state->period_ms = period_ms;

  // Without the ring the host's audio is dropped and the consumer still gets silence.
  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  state->ring = pbl_malloc(RING_SAMPLES * sizeof(int16_t));
  state->ring_read = 0;
  state->ring_count = 0;
  pbl_mutex_unlock(&state->lock);
  if (!state->ring) {
    PBL_LOG_WRN("No memory for host audio, capturing silence");
  }

  state->is_running = true;

  if (!new_timer_start(state->timer, period_ms, prv_timer_cb, (void *)this,
                       TIMER_START_FLAG_REPEATING)) {
    state->is_running = false;
    prv_free_ring(state);
    return false;
  }

  return true;
}

void mic_stop(MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);

  MicDeviceState *state = this->state;
  if (!state->is_running) {
    return;
  }

  state->is_running = false;
  new_timer_stop(state->timer);
  prv_free_ring(state);
  state->data_handler = nullptr;
  state->handler_context = nullptr;
  state->audio_buffer = nullptr;
  state->audio_buffer_len = 0;
}

bool mic_is_running(MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  return this->state->is_running;
}

uint32_t mic_get_channels(MicDevice *this) {
  PBL_ASSERTN(this);
  return this->channels ? this->channels : 1;
}
