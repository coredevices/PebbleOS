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
#include "drivers/audio.h"
#include <os/mutex.h>
#include <util/circular_buffer.h>

#include <stdbool.h>
#include <stdint.h>

#define CFG_AUDIO_PLAYBACK_PIPE_SIZE          (1024)

// Circular buffer configuration
#define CIRCULAR_BUF_SIZE_MS       (128)
#define CIRCULAR_BUF_SIZE_SAMPLES  ((MIC_SAMPLE_RATE * CIRCULAR_BUF_SIZE_MS) / 1000)
#define CIRCULAR_BUF_SIZE_BYTES    (CIRCULAR_BUF_SIZE_SAMPLES * sizeof(int16_t))

typedef enum AUDIO_PLL_STATE_TAG
{
    AUDIO_PLL_CLOSED,
    AUDIO_PLL_OPEN,
    AUDIO_PLL_ENABLE,
} AUDIO_PLL_STATE;

typedef struct AudioState {
  AUDCODEC_HandleTypeDef audcodec;
  AUDPRC_HandleTypeDef audprc;
  uint32_t slot_valid;
  uint8_t *queue_buf[HAL_AUDPRC_INSTANC_CNT];
  uint8_t *audec_queue_buf[HAL_AUDCODEC_INSTANC_CNT];
  AUDIO_PLL_STATE pll_state;
  uint32_t pll_samplerate;
  uint8_t tx_instanc;
  bool    tx_rbf_enable;
  uint16_t tx_buffer_size;
  uint8_t *circ_buffer_storage; 
  CircularBuffer circ_buffer;
  AudioTransCB trans_cb;
} AudioDeviceState;

typedef const struct AudioDevice {
  AudioDeviceState *state;
  uint32_t irq_priority; 
  DMA_Channel_TypeDef *audprc_dma_channel;
  uint32_t audprc_dma_request;
  IRQn_Type audprc_dma_irq;
  DMA_Channel_TypeDef *audec_dma_channel;
  uint32_t audec_dma_request;
  IRQn_Type audec_dma_irq;
  OutputConfig pa_ctrl;
  uint8_t data_format;
  uint8_t data_mode;
  uint32_t samplerate;
  uint32_t channels;
} AudioDevice;

extern void audprc_dma_iqr_handler(AudioDevice* audio_device);
extern void audec_dac0_dma_irq_handler(AudioDevice* audio_device);
