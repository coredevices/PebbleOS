/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mic_source.h"

#include <pbl/drivers/audio.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/system_task.h>
#include <pbl/util/circular_buffer.h>
#include <pbl/util/heap.h>

#include <kernel/kernel_heap.h>
#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DECLARE(service_mic_manager, CONFIG_SERVICE_MIC_MANAGER_LOG_LEVEL);

#define MS_PER_S       1000U
#define SAMPLES_PER_MS (MIC_MANAGER_SAMPLE_RATE / MS_PER_S)

#define PERIOD_MS     20U
#define PERIOD_FRAMES (MIC_MANAGER_SAMPLE_RATE * PERIOD_MS / MS_PER_S)
#define PERIODS       2U

// Queue between the device interrupt and the clients. The heap fragments, so a smaller one is
// taken when needed; transcription degrades below the minimum.
#define FIFO_MS        320U
#define FIFO_MIN_MS    128U
#define FIFO_STEP_MS   32U
#define FIFO_BYTES(ms) ((ms) * SAMPLES_PER_MS * MIC_MANAGER_CHANNELS * sizeof(int16_t))

// Frames per KernelBG callback, leaving time for Bluetooth to drain between batches
#define MAX_FRAMES_PER_CALLBACK 8U

static struct {
  struct pbl_mutex lock;
  bool initialized;
  bool running;
  volatile bool dispatch_pending;
  int16_t *ring;
  uint8_t *fifo_storage;
  CircularBuffer fifo;
  MicDataHandlerCB handler;
  void *context;
  int16_t *buffer;
  size_t buffer_len;
} s_src;

static void prv_dispatch(void *data);

static void prv_schedule_from_isr(void) {
  // A drop is retried on the next period; losing samples beats a full queue
  s_src.dispatch_pending = true;
  if (!system_task_add_callback_from_isr_droppable(prv_dispatch, nullptr)) {
    s_src.dispatch_pending = false;
  }
}

//! Runs from the device interrupt: queue the period, dropping the oldest samples when full.
static void prv_period_done(const struct pbl_audio *audio, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  const uint16_t bytes = PERIOD_FRAMES * MIC_MANAGER_CHANNELS * sizeof(int16_t);
  const uint16_t space = circular_buffer_get_write_space_remaining(&s_src.fifo);

  if (space < bytes) {
    circular_buffer_consume(&s_src.fifo, bytes - space);
  }
  (void)circular_buffer_write(&s_src.fifo, period, bytes);
  (void)pbl_audio_period_ready(audio, period);

  if (!s_src.dispatch_pending &&
      circular_buffer_get_read_space_remaining(&s_src.fifo) >= s_src.buffer_len * sizeof(int16_t)) {
    prv_schedule_from_isr();
  }
}

static void prv_dispatch(void *data) {
  const uint16_t frame_bytes = s_src.buffer_len * sizeof(int16_t);
  bool more = false;

  pbl_mutex_lock(&s_src.lock, PBL_FOREVER);

  for (uint32_t i = 0; i < MAX_FRAMES_PER_CALLBACK && s_src.running; i++) {
    pbl_irq_lock();
    const bool ready = circular_buffer_get_read_space_remaining(&s_src.fifo) >= frame_bytes;
    if (ready) {
      (void)circular_buffer_copy(&s_src.fifo, s_src.buffer, frame_bytes);
      circular_buffer_consume(&s_src.fifo, frame_bytes);
    }
    pbl_irq_unlock();

    if (!ready) {
      break;
    }

    s_src.handler(s_src.buffer, s_src.buffer_len, s_src.context);
    system_task_watchdog_feed();
  }

  pbl_irq_lock();
  more = s_src.running && circular_buffer_get_read_space_remaining(&s_src.fifo) >= frame_bytes;
  s_src.dispatch_pending = more;
  pbl_irq_unlock();

  if (more && !system_task_add_callback_droppable(prv_dispatch, nullptr)) {
    s_src.dispatch_pending = false;
  }

  pbl_mutex_unlock(&s_src.lock);
}

static uint8_t *prv_alloc_fifo(uint16_t *size) {
  for (uint32_t ms = FIFO_MS; ms >= FIFO_MIN_MS; ms -= FIFO_STEP_MS) {
    uint8_t *storage = kernel_malloc(FIFO_BYTES(ms));

    if (storage != nullptr) {
      if (ms < FIFO_MS) {
        PBL_LOG_WRN("Mic queue fell back to %" PRIu32 " ms", ms);
      }
      *size = FIFO_BYTES(ms);
      return storage;
    }
  }

  unsigned int used, free_bytes, max_free;
  heap_calc_totals(kernel_heap_get(), &used, &free_bytes, &max_free);
  PBL_LOG_ERR("No memory for the mic queue (max_free %u B)", max_free);
  return nullptr;
}

static void prv_free_locked(void) {
  pbl_audio_buf_free(s_src.ring);
  kernel_free(s_src.fifo_storage);
  s_src.ring = nullptr;
  s_src.fifo_storage = nullptr;
}

static bool prv_start_locked(void) {
  struct pbl_audio_stream stream = {
    .rate = MIC_MANAGER_SAMPLE_RATE,
    .channels = MIC_MANAGER_CHANNELS,
    .period_frames = PERIOD_FRAMES,
    .periods = PERIODS,
    .period_done = prv_period_done,
  };
  uint16_t fifo_size;
  int ret;

  s_src.ring = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
  s_src.fifo_storage = s_src.ring != nullptr ? prv_alloc_fifo(&fifo_size) : nullptr;
  if (s_src.fifo_storage == nullptr) {
    prv_free_locked();
    return false;
  }

  circular_buffer_init(&s_src.fifo, s_src.fifo_storage, fifo_size);
  stream.buf = s_src.ring;
  s_src.dispatch_pending = false;
  s_src.running = true;

  ret = pbl_audio_start(MIC, &stream);
  if (ret != 0) {
    PBL_LOG_WRN("Mic failed to start (%d)", ret);
    s_src.running = false;
    prv_free_locked();
    return false;
  }

  return true;
}

bool mic_source_start(MicDataHandlerCB handler, void *context, int16_t *buffer, size_t buffer_len) {
  bool started;

  if (!s_src.initialized) {
    pbl_mutex_init(&s_src.lock);
    s_src.initialized = true;
  }

  pbl_mutex_lock(&s_src.lock, PBL_FOREVER);
  if (s_src.running) {
    pbl_mutex_unlock(&s_src.lock);
    return false;
  }

  s_src.handler = handler;
  s_src.context = context;
  s_src.buffer = buffer;
  s_src.buffer_len = buffer_len;
  started = prv_start_locked();
  pbl_mutex_unlock(&s_src.lock);

  return started;
}

void mic_source_stop(void) {
  if (!s_src.initialized) {
    return;
  }

  pbl_mutex_lock(&s_src.lock, PBL_FOREVER);
  if (s_src.running) {
    s_src.running = false;
    (void)pbl_audio_stop(MIC);
    prv_free_locked();
  }
  pbl_mutex_unlock(&s_src.lock);
}
