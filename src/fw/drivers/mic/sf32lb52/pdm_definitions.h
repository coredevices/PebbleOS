/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"
#include "drivers/mic.h"
#include <os/mutex.h>
#include <util/circular_buffer.h>

#include <stdbool.h>
#include <stdint.h>

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