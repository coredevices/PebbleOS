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

#pragma once

#include "board/board.h"
#include "drivers/mic.h"
#include <os/mutex.h>
#include <util/circular_buffer.h>

#include <stdbool.h>
#include <stdint.h>

// PDM Configuration
#define PDM_BUFFER_SIZE_SAMPLES    (320)
#define PDM_BUFFER_COUNT           (2)
#define PDM_GAIN_DEFAULT           (30)

// Circular buffer configuration
#define CIRCULAR_BUF_SIZE_MS       (128)
#define CIRCULAR_BUF_SIZE_SAMPLES  ((MIC_SAMPLE_RATE * CIRCULAR_BUF_SIZE_MS) / 1000)
#define CIRCULAR_BUF_SIZE_BYTES    (CIRCULAR_BUF_SIZE_SAMPLES * sizeof(int16_t))

// Note: If the MCU has cache, this needs to be placed in DMA_BSS.
typedef struct MicState {
  uint8_t *circ_buffer_storage; 
  CircularBuffer circ_buffer;
  DMA_HandleTypeDef hdma;

  // User interface
  MicDataHandlerCB data_handler;
  void *handler_context;
  int16_t *audio_buffer;
  size_t audio_buffer_len;

  bool is_initialized;
  bool is_running;
  bool main_pending;
  bool bg_pending;
  uint16_t volume;

  // A mutex is needed to protect against a race condition between
  // mic_stop and the dispatch routine potentially resulting in the
  // deallocation of the subscriber module's receive buffer while the
  // dispatch routine is still running.
  PebbleRecursiveMutex *mutex;
  PDM_HandleTypeDef *hpdm;
} MicDeviceState;

typedef const struct MicDevice {
  MicDeviceState *state;
  PDM_TypeDef* pdm_instance;
  IRQn_Type pdm_irq;
  uint32_t pdm_irq_priority;
  IRQn_Type pdm_dma_irq;
  Pinmux clk_gpio;
  Pinmux data_gpio;
  uint32_t channels;
  uint32_t sample_rate;
  uint32_t channel_depth;
  // Volume scalar (max 128)
  uint16_t default_volume;
} MicDevice;

extern void pdm1_data_handler(MicDevice *this);
extern void pdm1_l_dma_handler(MicDevice *this);