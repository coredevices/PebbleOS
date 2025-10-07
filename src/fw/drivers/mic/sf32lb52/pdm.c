/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "drivers/mic.h"
#include "board/board.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "kernel/events.h"
#include "os/mutex.h"
#include "system/passert.h"
#include "util/circular_buffer.h"
#include "kernel/util/sleep.h"
#include "pdm_definitions.h"
#include "services/common/system_task.h"
#include "FreeRTOS.h"

#define PDM_AUDIO_RECORD_PIPE_SIZE         (288)
#define PDM_AUDIO_RECORD_GAIN_DEFAULT      (100)
#define PDM_AUDIO_RECORD_GAIN_MAX          (127)

// PDM Configuration
#define PDM_BUFFER_SIZE_SAMPLES            (320)
#define PDM_CH_COUNT                       (2)

// Circular buffer configuration
#define PDM_CIRCULAR_BUF_SIZE_MS           (128)
#define PDM_CIRCULAR_BUF_SIZE_SAMPLES      ((MIC_SAMPLE_RATE * PDM_CIRCULAR_BUF_SIZE_MS) / 1000)
#define PDM_CIRCULAR_BUF_SIZE_BYTES        (PDM_CIRCULAR_BUF_SIZE_SAMPLES * sizeof(int16_t) * PDM_CH_COUNT)

static PDM_HandleTypeDef s_hpdm;
static MicDeviceState* s_state;

void mic_init(const MicDevice *this) {
  PBL_ASSERTN(this);
  
  MicDeviceState *state = this->state;
  s_state = this->state;
  if (state && state->is_initialized) {
    return;
  }

  // Create mutex for thread safety
  state->mutex = mutex_create_recursive();
  PBL_ASSERTN(state->mutex);
  
  //Pinmux configuration
  HAL_PIN_Set(this->clk_gpio.pad, this->clk_gpio.func, this->clk_gpio.flags, 1);
  HAL_PIN_Set(this->data_gpio.pad, this->data_gpio.func, this->data_gpio.flags, 1);

  this->state->hpdm = &s_hpdm;
  PDM_HandleTypeDef* hpdm = this->state->hpdm;
  //HPDM configuration
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

void mic_set_volume(const MicDevice *this, uint16_t volume) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  
  MicDeviceState *state = this->state;
  PDM_HandleTypeDef* hpdm = state->hpdm;
  
  if (state->is_running) {
    PBL_LOG(LOG_LEVEL_WARNING, "Cannot set volume while microphone is running");
    return;
  }
  // volume form 0~127
  if(volume > PDM_AUDIO_RECORD_GAIN_MAX) volume = PDM_AUDIO_RECORD_GAIN_MAX;
  HAL_PDM_Set_Gain(hpdm, this->channels, volume);
}

static bool prv_allocate_buffers(MicDeviceState *state) {
  // Allocate circular buffer storage
  state->circ_buffer_storage = kernel_malloc(PDM_CIRCULAR_BUF_SIZE_BYTES);
  if (!state->circ_buffer_storage) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to allocate circular buffer storage");
    return false;
  }
  
  // Initialize circular buffer with allocated storage
  circular_buffer_init(&state->circ_buffer, state->circ_buffer_storage, PDM_CIRCULAR_BUF_SIZE_BYTES);
  
  return true;
}

static void prv_free_buffers(MicDeviceState *state) {
  // Free circular buffer storage
  if (state->circ_buffer_storage) {
    kernel_free(state->circ_buffer_storage);
    state->circ_buffer_storage = NULL;
  }
}

static void prv_dispatch_samples_common(void) {
  mutex_lock_recursive(s_state->mutex);

  // Only process if we have exactly one complete frame available and buffers are allocated
  if (s_state->is_running && s_state->data_handler && s_state->audio_buffer && s_state->circ_buffer_storage) {
    
    // Check if we have enough data for exactly one frame
    size_t frame_size_bytes = s_state->audio_buffer_len * sizeof(int16_t);
    
    // Use the circular buffer API to check available data
    uint16_t available_data = circular_buffer_get_read_space_remaining(&s_state->circ_buffer);
    
    while (available_data >= frame_size_bytes) {
      // Copy exactly one frame
      uint16_t bytes_copied = circular_buffer_copy(&s_state->circ_buffer,
          (uint8_t *)s_state->audio_buffer,
          frame_size_bytes);

      if (bytes_copied == frame_size_bytes) {
        // Call callback with exactly one frame
        s_state->data_handler(s_state->audio_buffer, s_state->audio_buffer_len, s_state->handler_context);
        
        // Consume exactly the frame we processed
        circular_buffer_consume(&s_state->circ_buffer, bytes_copied);
      }
      available_data -= bytes_copied;
    }
  }
  
  mutex_unlock_recursive(s_state->mutex);
}

static void prv_dispatch_samples_main(void *data) {  
  // Defensive check and clear pending flag
  if (!s_state || !s_state->is_initialized) {
    return;
  }
  
  // Always clear the pending flag, even if we can't process
  s_state->main_pending = false;
  
  // Only process if still running and properly initialized
  if (s_state->is_running) {
    prv_dispatch_samples_common();
  }
}

static void prv_dma_data_processing(uint8_t* data, uint16_t size)
{
  // Don't assert on is_running during shutdown - the PDM might send final events
  if (!s_state->is_running) {
    PBL_LOG(LOG_LEVEL_ERROR, "Microphone stopped, ignoring event");
    return;
  }

   // Ensure circular buffer storage is allocated
   if (!s_state->circ_buffer_storage) {
    PBL_LOG(LOG_LEVEL_ERROR, "No circular buffer storage, ignoring data");
    return;
  }
  
  // Ensure we have valid audio buffer info
  if (!s_state->audio_buffer || s_state->audio_buffer_len == 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "No audio buffer configured, ignoring data");
    return;
  }
  
  // Write samples to circular buffer
  if (!circular_buffer_write(&s_state->circ_buffer, data, size)) {
    PBL_LOG(LOG_LEVEL_ERROR, "circular buffer full");
    return;  // Buffer is full, drop remaining samples
  }

  // Check if we have enough data for a complete frame
  size_t frame_size_bytes = s_state->audio_buffer_len * sizeof(int16_t);
  uint16_t available_data = circular_buffer_get_read_space_remaining(&s_state->circ_buffer);
  if (available_data >= frame_size_bytes  && !s_state->main_pending) {
    s_state->main_pending = true;
    PebbleEvent e = {
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {
        .callback = prv_dispatch_samples_main,
        .data = NULL
      }
    };
    
    if (!event_put_isr(&e)) {
      s_state->main_pending = false;
    }
  }
}

void HAL_PDM_RxCpltCallback(PDM_HandleTypeDef *hpdm)
{
  prv_dma_data_processing(hpdm->pRxBuffPtr + (hpdm->RxXferSize / 2), hpdm->RxXferSize / 2);
}

void HAL_PDM_RxHalfCpltCallback(PDM_HandleTypeDef *hpdm)
{
  prv_dma_data_processing(hpdm->pRxBuffPtr, hpdm->RxXferSize / 2);
}

void pdm1_data_handler(MicDevice *this)
{
  HAL_PDM_IRQHandler(this->state->hpdm);
}

void pdm1_l_dma_handler(MicDevice *this)
{
  HAL_DMA_IRQHandler(this->state->hpdm->hdmarx);
}

static bool prv_start_pdm_capture(const MicDevice *this)
{
  PDM_HandleTypeDef* hpdm = this->state->hpdm;

  HAL_StatusTypeDef res;
  HAL_RCC_EnableModule(RCC_MOD_PDM1);
  res = HAL_PDM_Init(hpdm);
  if (this->channels ==1) {
    hpdm->Init.Channels = PDM_CHANNEL_LEFT_ONLY;
  } else {
    hpdm->Init.Channels = PDM_CHANNEL_STEREO;
  }
  hpdm->Init.SampleRate = this->sample_rate;
  hpdm->Init.ChannelDepth = (uint32_t) this->channel_depth;
  HAL_PDM_Config(hpdm, PDM_CFG_CHANNEL | PDM_CFG_SAMPLERATE | PDM_CFG_DEPTH);
  HAL_PDM_Set_Gain(hpdm, PDM_CHANNEL_STEREO, PDM_AUDIO_RECORD_GAIN_DEFAULT);

  // 3.072M = 49.152M(audpll)/16, 96k sampling use 3.072M as clock.
  if (hpdm->Init.clkSrc == 3072000 || hpdm->Init.SampleRate == PDM_SAMPLE_96KHZ)
  {
    bf0_enable_pll(hpdm->Init.SampleRate, 0);
  }
  HAL_NVIC_EnableIRQ(this->pdm_dma_irq);
  HAL_NVIC_EnableIRQ(this->pdm_irq);
  res |= HAL_PDM_Receive_DMA(hpdm, hpdm->pRxBuffPtr, hpdm->RxXferSize);

  return !res;
}

bool mic_start(const MicDevice *this, MicDataHandlerCB data_handler, void *context,
               int16_t *audio_buffer, size_t audio_buffer_len) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  PBL_ASSERTN(data_handler);
  PBL_ASSERTN(audio_buffer);
  PBL_ASSERTN(audio_buffer_len > 0);
  
  MicDeviceState *state = this->state;
  PDM_HandleTypeDef* hpdm = this->state->hpdm;
  
  mutex_lock_recursive(state->mutex);
  
  if (state->is_running) {
    mutex_unlock_recursive(state->mutex);
    return false;
  }
  if (!state->is_initialized) {
    PBL_LOG(LOG_LEVEL_ERROR, "Microphone not initialized");
    mutex_unlock_recursive(state->mutex);
    return false;
  }
  // Allocate buffers dynamically
  if (!prv_allocate_buffers(state)) {
    mutex_unlock_recursive(state->mutex);
    return false;
  }

  hpdm->RxXferSize = this->channels * PDM_AUDIO_RECORD_PIPE_SIZE * sizeof(int16_t);
  hpdm->pRxBuffPtr = kernel_malloc(hpdm->RxXferSize);
  PBL_ASSERT(hpdm->pRxBuffPtr, "Can not allocate buffer");

  // Reset state
  circular_buffer_init(&state->circ_buffer, state->circ_buffer_storage, PDM_CIRCULAR_BUF_SIZE_BYTES);
  state->data_handler = data_handler;
  state->handler_context = context;
  state->audio_buffer = audio_buffer;
  state->audio_buffer_len = audio_buffer_len;
  state->main_pending = false;
  
  // Set is_running to true BEFORE starting PDM, since the event handler will be called immediately
  state->is_running = true;
  // Start PDM capture
  if (!prv_start_pdm_capture(this)) {
    state->is_running = false;  // Reset on failure    
    prv_free_buffers(state);
    mutex_unlock_recursive(state->mutex);
    return false;
  }
    
  mutex_unlock_recursive(state->mutex);
  return true;
}

void mic_stop(const MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  
  MicDeviceState *state = this->state;
  PDM_HandleTypeDef* hpdm = this->state->hpdm;
  
  mutex_lock_recursive(state->mutex);
  
  if (!state->is_running) {
    mutex_unlock_recursive(state->mutex);
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

  kernel_free(hpdm->pRxBuffPtr);
  hpdm->pRxBuffPtr = NULL;
  
  // Clear state
  state->data_handler = NULL;
  state->handler_context = NULL;
  state->audio_buffer = NULL;
  state->audio_buffer_len = 0;
  state->main_pending = false;
    
  mutex_unlock_recursive(state->mutex);
}

#include "console/prompt.h"
#include "console/console_internal.h"

void command_mic_start(char *timeout_str, char *sample_size_str, char *sample_rate_str, char *format_str) {
  prompt_send_response("Microphone console commands not supported");
  prompt_send_response("Use the standard microphone API instead");
}

void command_mic_read(void) {
  prompt_send_response("Microphone read command not supported");
  prompt_send_response("Use the standard microphone API instead");
}

bool mic_is_running(const MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  
  return this->state->is_running;
}
