/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/mic.h>
#include <pbl/drivers/pmic/npm1300.h>
#include "board/board.h"
#include "kernel/kernel_heap.h"
#include "kernel/pbl_malloc.h"
#include "pbl/mcu/cache.h"
#include <pbl/logging/logging.h>
#include "pbl/kernel/mutex.h"
#include "pbl/kernel/irq.h"
#include "pbl/kernel/sched.h"
#include "system/passert.h"
#include "pbl/util/circular_buffer.h"
#include "pbl/util/heap.h"
#include "pbl/soc/sf32lb/sleep.h"
#include <pbl/drivers/mic/sf32lb52/pdm_definitions.h>
#include "pbl/services/system_task.h"

#include <inttypes.h>

PBL_LOG_MODULE_DEFINE(driver_mic_sf32lb, CONFIG_DRIVER_MIC_LOG_LEVEL);

// HACK alert, we need proper regulator abstraction
#if defined(CONFIG_BOARD_OBELIX) || defined(CONFIG_BOARD_GETAFIX)
#define PDM_POWER_NPM1300_LDO2 1
#endif

#define PDM_AUDIO_RECORD_PIPE_SIZE    (288)
#define PDM_CAPTURE_RESYNC_SAMPLES    (MIC_SAMPLE_RATE * 8 / 1000)
#define PDM_AUDIO_RECORD_GAIN_DEFAULT (90)
#define PDM_AUDIO_RECORD_GAIN_MAX     (120)

// PDM Configuration
#define PDM_BUFFER_SIZE_SAMPLES (320)

// Circular buffer configuration
#define PDM_CIRCULAR_BUF_SIZE_MS      (320)
#define PDM_CIRCULAR_BUF_SIZE_SAMPLES ((MIC_SAMPLE_RATE * PDM_CIRCULAR_BUF_SIZE_MS) / 1000)

// Minimum fallback size
// If it is any smaller than this, the transcription wont work well
#define PDM_CIRCULAR_BUF_MIN_SIZE_MS      (128)
#define PDM_CIRCULAR_BUF_MIN_SIZE_SAMPLES ((MIC_SAMPLE_RATE * PDM_CIRCULAR_BUF_MIN_SIZE_MS) / 1000)

// Fallback step. 32 ms shrink per retry gives us ~7 attempts between 320 ms and 128 ms
#define PDM_CIRCULAR_BUF_STEP_MS      (32)
#define PDM_CIRCULAR_BUF_STEP_SAMPLES ((MIC_SAMPLE_RATE * PDM_CIRCULAR_BUF_STEP_MS) / 1000)

#define PDM_POLLING_BUF_SIZE_MS          (128)
#define PDM_POLLING_BUF_SIZE_SAMPLES     ((MIC_SAMPLE_RATE * PDM_POLLING_BUF_SIZE_MS) / 1000)
#define PDM_POLLING_BUF_MIN_SIZE_MS      (64)
#define PDM_POLLING_BUF_MIN_SIZE_SAMPLES ((MIC_SAMPLE_RATE * PDM_POLLING_BUF_MIN_SIZE_MS) / 1000)

#define PDM_CIRCULAR_BUF_BYTES(samples, channels) ((size_t)(samples) * sizeof(int16_t) * (channels))

static PDM_HandleTypeDef s_hpdm;
static MicDeviceState *s_state;

void mic_init(const MicDevice *this) {
  PBL_ASSERTN(this);

  MicDeviceState *state = this->state;
  s_state = this->state;
  if (state && state->is_initialized) {
    return;
  }

#if PDM_POWER_NPM1300_LDO2
  (void)NPM1300_OPS.ldo2_set_enabled(false);
#endif

  pbl_mutex_init(&state->mutex);
  state->volume = PDM_AUDIO_RECORD_GAIN_DEFAULT;

  // Pinmux configuration
  HAL_PIN_Set(this->clk_gpio.pad, this->clk_gpio.func, this->clk_gpio.flags, 1);
  HAL_PIN_Set(this->data_gpio.pad, this->data_gpio.func, this->data_gpio.flags, 1);

  this->state->hpdm = &s_hpdm;
  PDM_HandleTypeDef *hpdm = this->state->hpdm;
  // HPDM configuration
  hpdm->Instance = this->pdm_instance;
  hpdm->hdmarx = &state->hdma;
  hpdm->Init.Mode = PDM_MODE_LOOP;
  hpdm->Init.Channels = this->channels;
  hpdm->Init.SampleRate = this->sample_rate;
  hpdm->Init.ChannelDepth = this->channel_depth;
  hpdm->Init.clkSrc = 9600000;
  HAL_NVIC_SetPriority(this->pdm_irq, this->pdm_irq_priority, 0);

  state->is_initialized = true;
}

// volume from 0~100
void mic_set_volume(const MicDevice *this, uint16_t volume) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);

  MicDeviceState *state = this->state;
  if (state->is_running) {
    PBL_LOG_WRN("Cannot set volume while microphone is running");
    return;
  }
  volume = volume * PDM_AUDIO_RECORD_GAIN_MAX / 100;
  // volume form 0~120 on HAL
  if (volume > PDM_AUDIO_RECORD_GAIN_MAX)
    volume = PDM_AUDIO_RECORD_GAIN_MAX;
  state->volume = volume;
}

static bool prv_allocate_buffers(const MicDevice *this, bool polling) {
  MicDeviceState *state = this->state;
  const uint32_t channels = this->channels ? this->channels : 1;
  const size_t requested = PDM_CIRCULAR_BUF_BYTES(
      polling ? PDM_POLLING_BUF_SIZE_SAMPLES : PDM_CIRCULAR_BUF_SIZE_SAMPLES, channels);
  const size_t floor = PDM_CIRCULAR_BUF_BYTES(
      polling ? PDM_POLLING_BUF_MIN_SIZE_SAMPLES : PDM_CIRCULAR_BUF_MIN_SIZE_SAMPLES, channels);
  const size_t step = PDM_CIRCULAR_BUF_BYTES(PDM_CIRCULAR_BUF_STEP_SAMPLES, channels);

  size_t try_size = requested;
  uint8_t *storage = NULL;

  while (try_size >= floor) {
    storage = kernel_malloc(try_size);
    if (storage) {
      break;
    }
    try_size -= step;
  }

  if (!storage) {
    unsigned int used, free_bytes, max_free;
    heap_calc_totals(kernel_heap_get(), &used, &free_bytes, &max_free);
    PBL_LOG_ERR("Failed to allocate PDM circular buffer (min %u B, max_free %u B)", (unsigned)floor,
                max_free);
    return false;
  }

  if (try_size < requested) {
    unsigned int used, free_bytes, max_free;
    heap_calc_totals(kernel_heap_get(), &used, &free_bytes, &max_free);
    PBL_LOG_WRN("PDM circular buffer fell back to %u B (requested %u, max_free %u)",
                (unsigned)try_size, (unsigned)requested, max_free);
  }

  state->circ_buffer_storage = storage;
  circular_buffer_init(&state->circ_buffer, storage, (uint16_t)try_size);
  return true;
}

static void prv_free_buffers(MicDeviceState *state) {
  // Free circular buffer storage
  if (state->circ_buffer_storage) {
    kernel_free(state->circ_buffer_storage);
    state->circ_buffer_storage = NULL;
  }
}

// Process at most this many frames per system task callback to allow
// other tasks (especially Bluetooth) to run and prevent send buffer overflow
#define MAX_FRAMES_PER_SYSTEM_TASK_CALLBACK 5

static void prv_dispatch_samples_system_task(void *data);

static void prv_dispatch_samples(bool polling) {
  // Defensive check
  if (!s_state || !s_state->is_initialized) {
    return;
  }

  pbl_mutex_lock(&s_state->mutex, PBL_FOREVER);
  if (polling != (s_state->ready_handler != NULL)) {
    pbl_mutex_unlock(&s_state->mutex);
    return;
  }

  // Process a limited number of frames to provide backpressure
  if (s_state->is_running && s_state->data_handler && s_state->audio_buffer &&
      s_state->circ_buffer_storage) {
    size_t frame_size_bytes = s_state->audio_buffer_len * sizeof(int16_t);
    int frames_processed = 0;
    int frame_limit = polling ? 1 : MAX_FRAMES_PER_SYSTEM_TASK_CALLBACK;

    while (s_state->is_running && s_state->data_handler && frames_processed < frame_limit) {
      // Check if we have enough data for a complete frame
      pbl_irq_lock();
      uint16_t available_data = circular_buffer_get_read_space_remaining(&s_state->circ_buffer);

      if (available_data < frame_size_bytes) {
        pbl_irq_unlock();
        break; // Not enough data for another frame
      }

      // Copy one frame
      uint16_t bytes_copied = circular_buffer_copy(
          &s_state->circ_buffer, (uint8_t *)s_state->audio_buffer, frame_size_bytes);
      s_state->frame_time = s_state->capture_epoch + s_state->timed_samples -
                            available_data / (sizeof(int16_t) * s_state->channels);
      circular_buffer_consume(&s_state->circ_buffer, bytes_copied);
      s_state->dispatched_bytes += bytes_copied;
      pbl_irq_unlock();

      if (bytes_copied == frame_size_bytes) {
        // Call callback with the frame
        s_state->frame_time_valid = true;
        s_state->data_handler(s_state->audio_buffer, s_state->audio_buffer_len,
                              s_state->handler_context);
        s_state->frame_time_valid = false;

        frames_processed++;

        // Feed the system task watchdog periodically during long processing
        if (!polling) {
          system_task_watchdog_feed();
        }
      } else {
        break; // Failed to copy, stop processing
      }
    }

    // If we still have data available after processing, reschedule immediately
    pbl_irq_lock();
    uint16_t remaining_data = circular_buffer_get_read_space_remaining(&s_state->circ_buffer);
    bool more_frames = remaining_data >= frame_size_bytes && s_state->is_running;
    if (!polling) {
      if (more_frames) {
        // Keep ownership of the pending callback; never block on our own queue.
        s_state->main_pending = true;
        if (!system_task_add_callback_droppable(prv_dispatch_samples_system_task, NULL)) {
          s_state->main_pending = false;
        }
      } else {
        // Clear pending flag only if we're done processing
        s_state->main_pending = false;
      }
    }
    pbl_irq_unlock();
    if (polling && more_frames) {
      s_state->ready_handler(s_state->handler_context);
    }
  } else {
    // Clear pending flag if we can't process
    s_state->main_pending = false;
  }

  pbl_mutex_unlock(&s_state->mutex);
}

static void prv_dispatch_samples_system_task(void *data) {
  prv_dispatch_samples(false);
}

static void prv_dma_data_processing(uint8_t *data, uint16_t size) {
  // Don't assert on is_running during shutdown - the PDM might send final events
  if (!s_state->is_running) {
    PBL_LOG_ERR("Microphone stopped, ignoring event");
    return;
  }

  // Ensure circular buffer storage is allocated
  if (!s_state->circ_buffer_storage) {
    PBL_LOG_ERR("No circular buffer storage, ignoring data");
    return;
  }

  // Ensure we have valid audio buffer info
  if (!s_state->audio_buffer || s_state->audio_buffer_len == 0) {
    PBL_LOG_ERR("No audio buffer configured, ignoring data");
    return;
  }

  // PDM DMA writes straight to RAM, bypassing D-cache. Drop any stale lines so
  // the CPU re-fetches the freshly captured samples instead of pre-DMA contents.
  // Buffer base is cache-line aligned at allocation time and the half-buffer
  // stride is a multiple of the cache line size, so this invalidate cannot
  // straddle neighboring allocations.
  dcache_invalidate(data, size);

  // Write samples directly to circular buffer
  // If buffer is full, drop oldest data to make room for fresh audio
  uint16_t write_space = circular_buffer_get_write_space_remaining(&s_state->circ_buffer);
  if (write_space < size) {
    uint16_t to_drop = size - write_space;
    circular_buffer_consume(&s_state->circ_buffer, to_drop);
    s_state->dropped_bytes += to_drop;
  }
  circular_buffer_write(&s_state->circ_buffer, data, size);
  s_state->capture_bytes += size;

  const uint32_t samples = size / (sizeof(int16_t) * s_state->channels);
  const uint32_t now = pbl_ticks_to_ms(pbl_uptime_ticks()) * (MIC_SAMPLE_RATE / 1000);
  s_state->timed_samples += samples;
  const int32_t skew = (int32_t)(now - (s_state->capture_epoch + s_state->timed_samples));
  if (s_state->timed_samples == samples || skew < -PDM_CAPTURE_RESYNC_SAMPLES ||
      skew > PDM_CAPTURE_RESYNC_SAMPLES) {
    s_state->capture_epoch += skew;
  }

  // Check if we have enough data for a complete frame
  size_t frame_size_bytes = s_state->audio_buffer_len * sizeof(int16_t);
  uint16_t available_data = circular_buffer_get_read_space_remaining(&s_state->circ_buffer);
  if (available_data > s_state->peak_backlog) {
    s_state->peak_backlog = available_data;
  }
  if (available_data < frame_size_bytes) {
    return;
  }
  if (s_state->ready_handler) {
    s_state->ready_handler(s_state->handler_context);
  } else if (!s_state->main_pending) {
    s_state->main_pending = true;

    // Dispatch to system task instead of kernel event queue (matches asterix behavior).
    // A drop is retried on the next PDM buffer event; losing samples beats
    // resetting the system over a full queue.
    bool should_context_switch = false;
    if (!system_task_add_callback_from_isr_droppable(prv_dispatch_samples_system_task, NULL,
                                                     &should_context_switch)) {
      s_state->main_pending = false;
    }
  }
}

void HAL_PDM_RxCpltCallback(PDM_HandleTypeDef *hpdm) {
  prv_dma_data_processing(hpdm->pRxBuffPtr + (hpdm->RxXferSize / 2), hpdm->RxXferSize / 2);
}

void HAL_PDM_RxHalfCpltCallback(PDM_HandleTypeDef *hpdm) {
  prv_dma_data_processing(hpdm->pRxBuffPtr, hpdm->RxXferSize / 2);
}

void pdm1_data_handler(MicDevice *this) {
  HAL_PDM_IRQHandler(this->state->hpdm);
}

void pdm1_l_dma_handler(MicDevice *this) {
  HAL_DMA_IRQHandler(this->state->hpdm->hdmarx);
}

static bool prv_start_pdm_capture(const MicDevice *this) {
  PDM_HandleTypeDef *hpdm = this->state->hpdm;

  HAL_StatusTypeDef res;
  HAL_RCC_EnableModule(RCC_MOD_PDM1);
  res = HAL_PDM_Init(hpdm);
  if (this->channels == 1) {
    hpdm->Init.Channels = PDM_CHANNEL_LEFT_ONLY;
  } else {
    hpdm->Init.Channels = PDM_CHANNEL_STEREO;
  }
  hpdm->Init.SampleRate = this->sample_rate;
  hpdm->Init.ChannelDepth = (uint32_t)this->channel_depth;
  HAL_PDM_Config(hpdm, PDM_CFG_CHANNEL | PDM_CFG_SAMPLERATE | PDM_CFG_DEPTH);
  HAL_PDM_Set_Gain(hpdm, PDM_CHANNEL_STEREO, this->state->volume);

  // 3.072M = 49.152M(audpll)/16, 96k sampling use 3.072M as clock.
  if (hpdm->Init.clkSrc == 3072000 || hpdm->Init.SampleRate == PDM_SAMPLE_96KHZ) {
    bf0_enable_pll(hpdm->Init.SampleRate, 0);
  }
  HAL_NVIC_EnableIRQ(this->pdm_dma_irq);
  HAL_NVIC_EnableIRQ(this->pdm_irq);
  res |= HAL_PDM_Receive_DMA(hpdm, hpdm->pRxBuffPtr, hpdm->RxXferSize);

  return !res;
}

static bool prv_start(const MicDevice *this, MicDataHandlerCB data_handler, void *context,
                      int16_t *audio_buffer, size_t audio_buffer_len, MicDataReadyCB ready) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  PBL_ASSERTN(data_handler);
  PBL_ASSERTN(audio_buffer);
  PBL_ASSERTN(audio_buffer_len > 0);

  MicDeviceState *state = this->state;
  PDM_HandleTypeDef *hpdm = this->state->hpdm;

  pbl_mutex_lock(&state->mutex, PBL_FOREVER);

  if (state->is_running) {
    pbl_mutex_unlock(&state->mutex);
    return false;
  }
  if (!state->is_initialized) {
    PBL_LOG_ERR("Microphone not initialized");
    pbl_mutex_unlock(&state->mutex);
    return false;
  }
  hpdm->RxXferSize = this->channels * PDM_AUDIO_RECORD_PIPE_SIZE * sizeof(int16_t);
  // Over-allocate by one cache line so the DMA buffer can start on a line
  // boundary. dcache_invalidate() in the IRQ path would otherwise risk
  // destroying dirty bytes in lines shared with neighboring allocations.
  const size_t cache_align = dcache_line_size();
  state->raw_dma_buffer = kernel_malloc(hpdm->RxXferSize + cache_align - 1U);
  if (!state->raw_dma_buffer || !prv_allocate_buffers(this, ready != NULL)) {
    kernel_free(state->raw_dma_buffer);
    state->raw_dma_buffer = NULL;
    pbl_mutex_unlock(&state->mutex);
    return false;
  }
  hpdm->pRxBuffPtr = (uint8_t *)(((uintptr_t)state->raw_dma_buffer + cache_align - 1U) &
                                 ~(uintptr_t)(cache_align - 1U));

  state->data_handler = data_handler;
  state->ready_handler = ready;
  state->handler_context = context;
  state->audio_buffer = audio_buffer;
  state->audio_buffer_len = audio_buffer_len;
  state->main_pending = false;
  state->channels = this->channels ? this->channels : 1;
  state->capture_epoch = 0;
  state->timed_samples = 0;
  state->frame_time_valid = false;
  state->capture_bytes = 0;
  state->dispatched_bytes = 0;
  state->dropped_bytes = 0;
  state->peak_backlog = 0;

#if PDM_POWER_NPM1300_LDO2
  (void)NPM1300_OPS.ldo2_set_enabled(true);
#endif
  // Set is_running to true BEFORE starting PDM, since the event handler will be called immediately
  state->is_running = true;

  // Prevent CPU from entering deep sleep during audio capture
  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPWFI);

  // Start PDM capture
  if (!prv_start_pdm_capture(this)) {
    HAL_NVIC_DisableIRQ(this->pdm_dma_irq);
    HAL_NVIC_DisableIRQ(this->pdm_irq);
    HAL_PDM_DMAStop(hpdm);
    HAL_PDM_DeInit(hpdm);
    HAL_RCC_DisableModule(RCC_MOD_PDM1);

    kernel_free(state->raw_dma_buffer);
    state->raw_dma_buffer = NULL;
    hpdm->pRxBuffPtr = NULL;

    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
    state->is_running = false; // Reset on failure
#if PDM_POWER_NPM1300_LDO2
    (void)NPM1300_OPS.ldo2_set_enabled(false);
#endif
    prv_free_buffers(state);
    pbl_mutex_unlock(&state->mutex);
    return false;
  }

  pbl_mutex_unlock(&state->mutex);
  return true;
}

bool mic_start(const MicDevice *this, MicDataHandlerCB data_handler, void *context,
               int16_t *audio_buffer, size_t audio_buffer_len) {
  return prv_start(this, data_handler, context, audio_buffer, audio_buffer_len, NULL);
}

bool mic_start_polling(const MicDevice *this, MicDataHandlerCB data_handler, void *context,
                       int16_t *audio_buffer, size_t audio_buffer_len, MicDataReadyCB ready) {
  return ready && prv_start(this, data_handler, context, audio_buffer, audio_buffer_len, ready);
}

void mic_poll(const MicDevice *this) {
  prv_dispatch_samples(true);
}

bool mic_get_frame_time(const MicDevice *this, uint32_t *sample_time) {
  if (!this->state->frame_time_valid) {
    return false;
  }
  *sample_time = this->state->frame_time;
  return true;
}

void mic_stop(const MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);

  MicDeviceState *state = this->state;
  PDM_HandleTypeDef *hpdm = this->state->hpdm;

  pbl_mutex_lock(&state->mutex, PBL_FOREVER);

  if (!state->is_running) {
    pbl_mutex_unlock(&state->mutex);
    return;
  }

  // Mark as stopped first to prevent new buffer requests
  state->is_running = false;

  HAL_NVIC_DisableIRQ(this->pdm_dma_irq);
  HAL_NVIC_DisableIRQ(this->pdm_irq);
  HAL_PDM_DMAStop(hpdm);
  HAL_PDM_DeInit(hpdm);
  // Free dynamically allocated buffers
  prv_free_buffers(state);

  kernel_free(state->raw_dma_buffer);
  state->raw_dma_buffer = NULL;
  hpdm->pRxBuffPtr = NULL;

  // Clear state
  state->data_handler = NULL;
  state->ready_handler = NULL;
  state->handler_context = NULL;
  state->audio_buffer = NULL;
  state->audio_buffer_len = 0;
  state->main_pending = false;

#if PDM_POWER_NPM1300_LDO2
  (void)NPM1300_OPS.ldo2_set_enabled(false);
#endif

  // Allow CPU to enter deep sleep again
  soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);

  pbl_mutex_unlock(&state->mutex);
}

#include "console/prompt.h"

void command_mic_start(char *timeout_str, char *sample_size_str, char *sample_rate_str,
                       char *format_str) {
  prompt_send_response("Microphone console commands not supported");
  prompt_send_response("Use the standard microphone API instead");
}

void command_mic_read(void) {
  pbl_irq_lock();
  const uint32_t captured = s_state->capture_bytes;
  const uint32_t dispatched = s_state->dispatched_bytes;
  const uint32_t dropped = s_state->dropped_bytes;
  const unsigned backlog = s_state->peak_backlog;
  pbl_irq_unlock();
  char buffer[128];
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "mic captured=%" PRIu32 " dispatched=%" PRIu32 " dropped=%" PRIu32
                           " peak_backlog=%u",
                           captured, dispatched, dropped, backlog);
}

bool mic_is_running(const MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);

  return this->state->is_running;
}

uint32_t mic_get_channels(const MicDevice *this) {
  PBL_ASSERTN(this);
  return this->channels ? this->channels : 1;
}
