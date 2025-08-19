/*
 * Copyright 2025 Joshua Jun
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
#include "drivers/mic/nrf5/pdm_definitions.h"

#include "board/board.h"
#include "drivers/clocksource.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "os/mutex.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/circular_buffer.h"
#include "util/math.h"
#include "util/size.h"
#include "util/time/time.h"

#include "hal/nrf_clock.h"
#include "nrfx_pdm.h"

static void prv_pdm_event_handler(nrfx_pdm_evt_t const *p_evt);
static void prv_dispatch_samples_main(void *data);
static void prv_dispatch_samples_common(void);
static bool prv_allocate_buffers(MicDeviceState *state);
static void prv_free_buffers(MicDeviceState *state);

static bool prv_is_valid_buffer(MicDeviceState *state, int16_t *buffer) {
  for (int i = 0; i < PDM_BUFFER_COUNT; i++) {
    if (state->pdm_buffers[i] && buffer == state->pdm_buffers[i]) {
      return true;
    }
  }
  return false;
}

static bool prv_allocate_buffers(MicDeviceState *state) {
  // Allocate circular buffer storage
  state->circ_buffer_storage = kernel_malloc(CIRCULAR_BUF_SIZE_BYTES);
  if (!state->circ_buffer_storage) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to allocate circular buffer storage");
    return false;
  }
  
  // Allocate PDM buffers
  for (int i = 0; i < PDM_BUFFER_COUNT; i++) {
    state->pdm_buffers[i] = kernel_malloc(PDM_BUFFER_SIZE_SAMPLES * sizeof(int16_t));
    if (!state->pdm_buffers[i]) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to allocate PDM buffer %d", i);
      // Free any previously allocated buffers
      prv_free_buffers(state);
      return false;
    }
    // Clear the buffer
    memset(state->pdm_buffers[i], 0, PDM_BUFFER_SIZE_SAMPLES * sizeof(int16_t));
  }
  
  // Initialize circular buffer with allocated storage
  circular_buffer_init(&state->circ_buffer, state->circ_buffer_storage, CIRCULAR_BUF_SIZE_BYTES);
  
  return true;
}

static void prv_free_buffers(MicDeviceState *state) {
  // Free circular buffer storage
  if (state->circ_buffer_storage) {
    kernel_free(state->circ_buffer_storage);
    state->circ_buffer_storage = NULL;
  }
  
  // Free PDM buffers
  for (int i = 0; i < PDM_BUFFER_COUNT; i++) {
    if (state->pdm_buffers[i]) {
      kernel_free(state->pdm_buffers[i]);
      state->pdm_buffers[i] = NULL;
    }
  }
}

static void prv_process_pdm_buffer(MicDeviceState *state, int16_t *pdm_data) {
  // Ensure circular buffer storage is allocated
  if (!state->circ_buffer_storage) {
    return;
  }
  
  // Write samples to circular buffer
  for (int i = 0; i < PDM_BUFFER_SIZE_SAMPLES; i++) {
    if (!circular_buffer_write(&state->circ_buffer, 
                              (const uint8_t *)&pdm_data[i], 
                              sizeof(int16_t))) {
      break;  // Buffer is full, drop remaining samples
    }
  }
  
  // Check if we have enough data for a complete frame
  size_t frame_size_bytes = state->audio_buffer_len * sizeof(int16_t);
  uint16_t available_data = circular_buffer_get_read_space_remaining(&state->circ_buffer);
  
  if (available_data >= frame_size_bytes && !state->main_pending) {
    state->main_pending = true;
    PebbleEvent e = {
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {
        .callback = prv_dispatch_samples_main,
        .data = NULL
      }
    };
    
    if (!event_put_isr(&e)) {
      state->main_pending = false;
    }
  }
}

static void prv_pdm_event_handler(nrfx_pdm_evt_t const *p_evt) {
  MicDeviceState *state = MIC->state;
  
  PBL_ASSERTN(state->is_initialized);
  PBL_ASSERTN(state->is_running);
  PBL_ASSERTN(p_evt->error == NRFX_PDM_NO_ERROR);
  
  if (p_evt->buffer_requested) {
    uint8_t next_buffer_idx = (state->current_buffer_idx + 1) % PDM_BUFFER_COUNT;
    nrfx_err_t err = nrfx_pdm_buffer_set(&MIC->pdm_instance, 
                                        state->pdm_buffers[next_buffer_idx], 
                                        PDM_BUFFER_SIZE_SAMPLES);
    if (err == NRFX_SUCCESS) {
      state->current_buffer_idx = next_buffer_idx;
    }
  }
  
  if (p_evt->buffer_released) {
    int16_t *pdm_data = (int16_t *)p_evt->buffer_released;
    
    if (pdm_data && prv_is_valid_buffer(state, pdm_data)) {
      prv_process_pdm_buffer(state, pdm_data);
    }
  }
}

void mic_init(const MicDevice *this) {
  PBL_ASSERTN(this);
  
  MicDeviceState *state = this->state;
  
  if (state && state->is_initialized) {
    return;
  }

  memset(state, 0, sizeof(MicDeviceState));
  
  // Initialize PDM configuration
  state->pdm_config = (nrfx_pdm_config_t)NRFX_PDM_DEFAULT_CONFIG(this->clk_pin, this->data_pin);
  state->pdm_config.clock_freq = NRF_PDM_FREQ_1280K;
  state->pdm_config.ratio = NRF_PDM_RATIO_80X;
  
  // Create mutex for thread safety
  state->mutex = mutex_create_recursive();
  PBL_ASSERTN(state->mutex);
  
  // Initialize PDM driver once during init
  nrfx_err_t err = nrfx_pdm_init(&this->pdm_instance, &state->pdm_config, prv_pdm_event_handler);
  if (err != NRFX_SUCCESS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to initialize PDM: %d", err);
    return;
  }
  
  state->is_initialized = true;
}

static void prv_dispatch_samples_common(void) {
  MicDeviceState *state = MIC->state;
  
  mutex_lock_recursive(state->mutex);

  // Only process if we have exactly one complete frame available and buffers are allocated
  if (state->is_running && state->data_handler && state->audio_buffer && state->circ_buffer_storage) {
    
    // Check if we have enough data for exactly one frame
    size_t frame_size_bytes = state->audio_buffer_len * sizeof(int16_t);
    
    // Use the circular buffer API to check available data
    uint16_t available_data = circular_buffer_get_read_space_remaining(&state->circ_buffer);
    
    if (available_data >= frame_size_bytes) {
      // Copy exactly one frame
      uint16_t bytes_copied = circular_buffer_copy(&state->circ_buffer,
          (uint8_t *)state->audio_buffer,
          frame_size_bytes);

      if (bytes_copied == frame_size_bytes) {
        // Call callback with exactly one frame
        state->data_handler(state->audio_buffer, state->audio_buffer_len, state->handler_context);
        
        // Consume exactly the frame we processed
        circular_buffer_consume(&state->circ_buffer, bytes_copied);
      }
    }
  }
  
  mutex_unlock_recursive(state->mutex);
}

static void prv_dispatch_samples_main(void *data) {
  MicDeviceState *state = MIC->state;
  
  // Defensive check and clear pending flag
  if (!state || !state->is_initialized) {
    return;
  }
  
  // Always clear the pending flag, even if we can't process
  state->main_pending = false;
  
  // Only process if still running and properly initialized
  if (state->is_running) {
    prv_dispatch_samples_common();
  }
}

void mic_set_volume(const MicDevice *this, uint16_t volume) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  
  MicDeviceState *state = this->state;
  
  if (state->is_running) {
    PBL_LOG(LOG_LEVEL_WARNING, "Cannot set volume while microphone is running");
    return;
  }
  
  // Scale volume from 0-1024 range to nRF PDM gain range (0-80)
  // Volume 0 = minimum gain, 1024 = maximum gain
  uint16_t nrf_gain;
  if (volume == 0) {
    nrf_gain = NRF_PDM_GAIN_MINIMUM;
  } else if (volume >= 1024) {
    nrf_gain = NRF_PDM_GAIN_MAXIMUM;
  } else {
    // Linear scaling: volume * (max - min) / 1024 + min
    nrf_gain = (volume * (NRF_PDM_GAIN_MAXIMUM - NRF_PDM_GAIN_MINIMUM)) / 1024 + NRF_PDM_GAIN_MINIMUM;
  }
  
  state->pdm_config.gain_l = nrf_gain;
  state->pdm_config.gain_r = nrf_gain;
}

static bool prv_start_pdm_capture(const MicDevice *this) {
  MicDeviceState *state = this->state;
  
  // Clear buffers and set initial buffer
  for (int i = 0; i < PDM_BUFFER_COUNT; i++) {
    if (state->pdm_buffers[i]) {
      memset(state->pdm_buffers[i], 0, PDM_BUFFER_SIZE_SAMPLES * sizeof(int16_t));
    }
  }
  state->current_buffer_idx = 0;
  
  nrfx_err_t err = nrfx_pdm_buffer_set(&this->pdm_instance, 
                           state->pdm_buffers[0], 
                           PDM_BUFFER_SIZE_SAMPLES);
  if (err != NRFX_SUCCESS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to set initial PDM buffer: %d", err);
    return false;
  }
  
  // Start PDM capture
  err = nrfx_pdm_start(&this->pdm_instance);
  if (err != NRFX_SUCCESS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to start PDM: %d", err);
    return false;
  }
  
  return true;
}

bool mic_start(const MicDevice *this, MicDataHandlerCB data_handler, void *context,
               int16_t *audio_buffer, size_t audio_buffer_len) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  PBL_ASSERTN(data_handler);
  PBL_ASSERTN(audio_buffer);
  PBL_ASSERTN(audio_buffer_len > 0);
  
  MicDeviceState *state = this->state;
  
  mutex_lock_recursive(state->mutex);
  
  if (state->is_running) {
    PBL_LOG(LOG_LEVEL_WARNING, "Microphone is already running");
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
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to allocate microphone buffers");
    mutex_unlock_recursive(state->mutex);
    return false;
  }
  
  // Reset state
  circular_buffer_init(&state->circ_buffer, state->circ_buffer_storage, CIRCULAR_BUF_SIZE_BYTES);
  state->data_handler = data_handler;
  state->handler_context = context;
  state->audio_buffer = audio_buffer;
  state->audio_buffer_len = audio_buffer_len;
  state->main_pending = false;
  
  // Request high frequency crystal oscillator
  clocksource_hfxo_request();
  
  // Start PDM capture
  if (!prv_start_pdm_capture(this)) {
    clocksource_hfxo_release();
    prv_free_buffers(state);
    mutex_unlock_recursive(state->mutex);
    return false;
  }
  
  state->is_running = true;
  PBL_LOG(LOG_LEVEL_INFO, "Microphone started");
  
  mutex_unlock_recursive(state->mutex);
  return true;
}

void mic_stop(const MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  
  MicDeviceState *state = this->state;
  
  mutex_lock_recursive(state->mutex);
  
  if (!state->is_running) {
    mutex_unlock_recursive(state->mutex);
    return;
  }
  
  // Mark as stopped first to prevent new buffer requests
  state->is_running = false;
  
  // Stop PDM capture
  nrfx_pdm_stop(&this->pdm_instance);
  
  // Release high frequency oscillator
  clocksource_hfxo_release();
  
  // Free dynamically allocated buffers
  prv_free_buffers(state);
  
  // Clear state
  state->data_handler = NULL;
  state->handler_context = NULL;
  state->audio_buffer = NULL;
  state->audio_buffer_len = 0;
  state->main_pending = false;
  
  PBL_LOG(LOG_LEVEL_INFO, "Microphone stopped");
  
  mutex_unlock_recursive(state->mutex);
}

#include "console/prompt.h"
#include "console/console_internal.h"

// Console command stubs for Asterix (since we don't have accessory connector)
// These commands are defined in the console command table but Asterix doesn't need
// the full accessory-based microphone streaming functionality

void command_mic_start(char *timeout_str, char *sample_size_str, char *sample_rate_str, char *format_str) {
  prompt_send_response("Microphone console commands not supported on Asterix");
  prompt_send_response("Use the standard microphone API instead");
}

void command_mic_read(void) {
  prompt_send_response("Microphone read command not supported on Asterix");
  prompt_send_response("Use the standard microphone API instead");
}

bool mic_is_running(const MicDevice *this) {
  PBL_ASSERTN(this);
  PBL_ASSERTN(this->state);
  
  return this->state->is_running;
}
