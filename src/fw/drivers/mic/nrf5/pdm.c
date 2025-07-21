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

#include "board/board.h"
#include "drivers/nrf5/hfxo.h"
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

// PDM Configuration
#define PDM_BUFFER_SIZE_SAMPLES    (320)
#define PDM_BUFFER_COUNT           (2)
#define PDM_GAIN_DEFAULT           (NRF_PDM_GAIN_DEFAULT)

// Circular buffer configuration
#define CIRCULAR_BUF_SIZE_MS       (20)
#define CIRCULAR_BUF_SIZE_SAMPLES  ((MIC_SAMPLE_RATE * CIRCULAR_BUF_SIZE_MS) / 1000)
#define CIRCULAR_BUF_SIZE_BYTES    (CIRCULAR_BUF_SIZE_SAMPLES * sizeof(int16_t))

typedef struct {
  // PDM hardware
  nrfx_pdm_t pdm_instance;
  nrfx_pdm_config_t pdm_config;
  int16_t pdm_buffers[PDM_BUFFER_COUNT][PDM_BUFFER_SIZE_SAMPLES] __attribute__((aligned(4)));
  uint8_t current_buffer_idx;
  
  // User interface
  MicDataHandlerCB data_handler;
  void *handler_context;
  int16_t *audio_buffer;
  size_t audio_buffer_len;
  
  // Intermediate storage
  CircularBuffer circ_buffer;
  uint8_t circ_buffer_storage[CIRCULAR_BUF_SIZE_BYTES] __attribute__((aligned(4)));
  
  // State management
  PebbleRecursiveMutex *mutex;
  bool is_running;
  bool is_initialized;
  bool main_pending;
  
  // Volume control
  uint16_t volume_setting;
  int overflow_count;
} MicDeviceState;

// Forward declaration to match the header
struct MicDevice {
  MicDeviceState *state;
  
  // Hardware configuration
  uint32_t clk_pin;
  uint32_t data_pin;
};

static void prv_pdm_event_handler(nrfx_pdm_evt_t const *p_evt);
static void prv_dispatch_samples_main(void *data);
static void prv_dispatch_samples_common(void);

// Use static allocation to avoid memory allocation issues
static MicDeviceState s_mic_state_storage;
static MicDeviceState *s_mic_state = NULL;

static struct MicDevice s_mic_device = {
  .state = NULL,
  .clk_pin = NRF_GPIO_PIN_MAP(1, 0),   // P1.00 - PDM CLK
  .data_pin = NRF_GPIO_PIN_MAP(0, 24), // P0.24 - PDM DATA
};

const struct MicDevice * const MIC = &s_mic_device;

static bool prv_is_valid_buffer(MicDeviceState *state, int16_t *buffer) {
  for (int i = 0; i < PDM_BUFFER_COUNT; i++) {
    if (buffer == state->pdm_buffers[i]) {
      return true;
    }
  }
  return false;
}

static void prv_process_pdm_buffer(MicDeviceState *state, int16_t *pdm_data) {
  // Write samples to circular buffer
  for (int i = 0; i < PDM_BUFFER_SIZE_SAMPLES; i++) {
    if (!circular_buffer_write(&state->circ_buffer, 
                              (const uint8_t *)&pdm_data[i], 
                              sizeof(int16_t))) {
      state->overflow_count++;
      break;
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
  MicDeviceState *state = s_mic_device.state;
  
  if (!state || !p_evt || !state->is_initialized || !state->is_running) {
    return;
  }
  
  if (p_evt->error != NRFX_PDM_NO_ERROR) {
    return;
  }
  
  if (p_evt->buffer_requested) {
    uint8_t next_buffer_idx = (state->current_buffer_idx + 1) % PDM_BUFFER_COUNT;
    nrfx_err_t err = nrfx_pdm_buffer_set(&state->pdm_instance, 
                                        state->pdm_buffers[next_buffer_idx], 
                                        PDM_BUFFER_SIZE_SAMPLES);
    if (err == NRFX_SUCCESS) {
      state->current_buffer_idx = next_buffer_idx;
    }
  }
  
  if (p_evt->buffer_released) {
    int16_t *pdm_data = (int16_t *)p_evt->buffer_released;
    
    if (pdm_data && ((uintptr_t)pdm_data & 0x3) == 0 && 
        prv_is_valid_buffer(state, pdm_data)) {
      prv_process_pdm_buffer(state, pdm_data);
    }
  }
}

void mic_init(const MicDevice *this) {
  PBL_ASSERTN(this);
  
  if (s_mic_state && s_mic_state->is_initialized) {
    return;
  }

  s_mic_state = &s_mic_state_storage;
  memset(s_mic_state, 0, sizeof(MicDeviceState));
  s_mic_device.state = s_mic_state;
  
  // Initialize PDM instance and configuration
  s_mic_state->pdm_instance = (nrfx_pdm_t)NRFX_PDM_INSTANCE(0);
  s_mic_state->pdm_config = (nrfx_pdm_config_t)NRFX_PDM_DEFAULT_CONFIG(this->clk_pin, this->data_pin);
  s_mic_state->pdm_config.mode = NRF_PDM_MODE_MONO;
  s_mic_state->pdm_config.clock_freq = NRF_PDM_FREQ_1280K;
  s_mic_state->pdm_config.ratio = NRF_PDM_RATIO_80X;
  s_mic_state->pdm_config.gain_l = PDM_GAIN_DEFAULT;
  s_mic_state->pdm_config.gain_r = PDM_GAIN_DEFAULT;
  
  s_mic_state->volume_setting = PDM_GAIN_DEFAULT;
  
  // Initialize circular buffer
  circular_buffer_init(&s_mic_state->circ_buffer, 
                      s_mic_state->circ_buffer_storage, 
                      sizeof(s_mic_state->circ_buffer_storage));
  
  // Create mutex for thread safety
  s_mic_state->mutex = mutex_create_recursive();
  PBL_ASSERTN(s_mic_state->mutex);
  
  s_mic_state->is_initialized = true;
}

static void prv_dispatch_samples_common(void) {
  MicDeviceState *state = s_mic_device.state;
  
  if (!state || !state->mutex) {
    return;
  }
  
  mutex_lock_recursive(state->mutex);

  // Only process if we have exactly one complete frame available
  if (state->is_running && state->data_handler && state->audio_buffer) {
    
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
        uintptr_t func_addr = (uintptr_t)state->data_handler;
        if (func_addr > 0x1000 && func_addr < 0x100000) {
          state->data_handler(state->audio_buffer, state->audio_buffer_len, state->handler_context);
        }
        
        // Consume exactly the frame we processed
        circular_buffer_consume(&state->circ_buffer, bytes_copied);
      }
    }
  }
  
  mutex_unlock_recursive(state->mutex);
}

static void prv_dispatch_samples_main(void *data) {
  MicDeviceState *state = s_mic_device.state;
  
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
  
  // Clamp volume to valid PDM gain range
  if (volume > NRF_PDM_GAIN_MAXIMUM) {
    volume = NRF_PDM_GAIN_MAXIMUM;
  }
  
  state->volume_setting = volume;
  state->pdm_config.gain_l = volume;
  state->pdm_config.gain_r = volume;
}

static bool prv_init_pdm_hardware(MicDeviceState *state) {
  // Initialize PDM driver
  nrfx_err_t err = nrfx_pdm_init(&state->pdm_instance, &state->pdm_config, prv_pdm_event_handler);
  if (err != NRFX_SUCCESS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to initialize PDM: %d", err);
    return false;
  }
  
  // Clear and set initial buffer
  memset(state->pdm_buffers, 0, sizeof(state->pdm_buffers));
  state->current_buffer_idx = 0;
  
  err = nrfx_pdm_buffer_set(&state->pdm_instance, 
                           state->pdm_buffers[0], 
                           PDM_BUFFER_SIZE_SAMPLES);
  if (err != NRFX_SUCCESS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to set initial PDM buffer: %d", err);
    nrfx_pdm_uninit(&state->pdm_instance);
    return false;
  }
  
  // Start PDM capture
  err = nrfx_pdm_start(&state->pdm_instance);
  if (err != NRFX_SUCCESS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to start PDM: %d", err);
    nrfx_pdm_uninit(&state->pdm_instance);
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
  
  // Reset state
  circular_buffer_init(&state->circ_buffer, state->circ_buffer_storage, sizeof(state->circ_buffer_storage));
  state->data_handler = data_handler;
  state->handler_context = context;
  state->audio_buffer = audio_buffer;
  state->audio_buffer_len = audio_buffer_len;
  state->overflow_count = 0;
  state->main_pending = false;
  
  // Request high frequency crystal oscillator
  nrf52_clock_hfxo_request();
  
  // Initialize and start PDM hardware
  if (!prv_init_pdm_hardware(state)) {
    nrf52_clock_hfxo_release();
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
  nrfx_pdm_stop(&state->pdm_instance);
  nrfx_pdm_uninit(&state->pdm_instance);
  
  // Give time for pending ISRs to complete
  psleep(1);
  
  // Release high frequency oscillator
  nrf52_clock_hfxo_release();
  
  // Clear state
  state->data_handler = NULL;
  state->handler_context = NULL;
  state->audio_buffer = NULL;
  state->audio_buffer_len = 0;
  state->main_pending = false;
  
  PBL_LOG(LOG_LEVEL_INFO, "Microphone stopped, overflow count: %d", state->overflow_count);
  
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
