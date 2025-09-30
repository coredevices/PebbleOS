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

#pragma once

#include "board/board.h"
#include "drivers/mic.h"
#include "os/mutex.h"
#include "util/circular_buffer.h"

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_pdm.h"

// PDM Configuration
#define PDM_BUFFER_SIZE_SAMPLES    (320)
#define PDM_BUFFER_COUNT           (2)
#define PDM_GAIN_DEFAULT           (NRF_PDM_GAIN_DEFAULT)

// Circular buffer configuration
#define CIRCULAR_BUF_SIZE_MS       (320)
#define CIRCULAR_BUF_SIZE_SAMPLES  ((MIC_SAMPLE_RATE * CIRCULAR_BUF_SIZE_MS) / 1000)
#define CIRCULAR_BUF_SIZE_BYTES    (CIRCULAR_BUF_SIZE_SAMPLES * sizeof(int16_t))

typedef struct {
  nrfx_pdm_config_t pdm_config;
  int16_t *pdm_buffers[PDM_BUFFER_COUNT];
  uint8_t current_buffer_idx;
  
  // User interface
  MicDataHandlerCB data_handler;
  void *handler_context;
  int16_t *audio_buffer;
  size_t audio_buffer_len;
  
  // Intermediate storage
  CircularBuffer circ_buffer;
  uint8_t *circ_buffer_storage;
  
  // State management
  PebbleRecursiveMutex *mutex;
  bool is_running;
  bool is_initialized;
  bool main_pending;
} MicDeviceState;

typedef const struct MicDevice {
  MicDeviceState *state;
  
  // Hardware configuration
  const nrfx_pdm_t pdm_instance;
  uint32_t clk_pin;
  uint32_t data_pin;
} MicDevice;
