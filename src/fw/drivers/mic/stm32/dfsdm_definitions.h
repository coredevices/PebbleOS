/*
 * Copyright 2024 Google LLC
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

#define DFSDM_BUFFER_LENGTH  (256)  // Each buffer fills up 64 times per second
#define DFSDM_CIRC_BUFFER_SIZE  (DFSDM_BUFFER_LENGTH * sizeof(uint16_t) * 2)

// Note: If the MCU has cache, this needs to be placed in DMA_BSS.
typedef struct MicState {
  int32_t in_buffer[2][DFSDM_BUFFER_LENGTH];      // 2k
  uint8_t circ_buf_store[DFSDM_CIRC_BUFFER_SIZE];  // 1k
  CircularBuffer circ_buffer;

  bool initialized;
  bool running;
  bool main_pending;
  bool bg_pending;
  uint32_t overflow_cnt;
  uint32_t bytes_received;
  int samples_to_discard;

  int64_t hpf_y1; // Previous value of HPF output
  int16_t prev_r; // Previous random number generated for dithering

  uint16_t volume;

  // A mutex is needed to protect against a race condition between
  // mic_stop and the dispatch routine potentially resulting in the
  // deallocation of the subscriber module's receive buffer while the
  // dispatch routine is still running.
  PebbleRecursiveMutex *mic_mutex;

  struct MicSubscriber {
    MicDataHandlerCB callback;
    int16_t *buffer;
    void *context;
    size_t size;
    size_t idx;
  } subscriber;
} MicDeviceState;

typedef const struct MicDevice {
  MicDeviceState *state;

  DFSDM_TypeDef *filter;
  DFSDM_Channel_TypeDef *channel;
  uint32_t extremes_detector_channel;
  uint32_t regular_channel;
  uint32_t pdm_frequency;
  uint32_t rcc_apb_periph;
  DMARequest *dma;
  AfConfig ck_gpio;
  AfConfig sd_gpio;

  int power_on_delay_ms;
  int settling_delay_ms;

  // Volume scalar (max 256)
  uint16_t default_volume;

  // Final right shift after applying gain control. It should be adjusted per watch family such that
  // a volume of 128 provides approximate half of full-scale deflection for normal speech on a
  // bigboard
  int final_right_shift;

  //! Function pointer to power the microphone hardware on or off.
  //! May be NULL.
  void (*mic_power_state_fn)(bool enabled);
} MicDevice;
