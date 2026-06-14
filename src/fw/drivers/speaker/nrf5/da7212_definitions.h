/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"
#include "drivers/audio.h"
#include "util/circular_buffer.h"

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_i2s.h"

// Number of mono samples per I2S half-buffer. The stereo buffer holds 2x this
// many 16-bit samples. With SAMPLE_RATE=16000, 512 mono samples = 32ms.
#define NRF5_AUDIO_I2S_BUF_SAMPLES_MONO   512
#define NRF5_AUDIO_I2S_BUF_COUNT          2

// Mono bytes held in the circular buffer (caller <-> DMA). 4096 bytes = 2048
// mono samples = 128ms at 16kHz.
#define NRF5_AUDIO_CIRC_BUF_SIZE_BYTES    4096

// Request a refill from the caller whenever this much mono-data space is free.
#define NRF5_AUDIO_REFILL_THRESHOLD_BYTES 1024

typedef struct AudioDeviceState {
  AudioTransCB trans_cb;
  bool is_running;
  bool callback_pending;
  uint8_t buf_idx;

  int16_t *i2s_bufs[NRF5_AUDIO_I2S_BUF_COUNT];

  uint8_t *circ_buffer_storage;
  CircularBuffer circ_buffer;
} AudioDeviceState;

typedef const struct AudioDevice {
  AudioDeviceState *state;

  nrfx_i2s_t i2s_instance;
  uint32_t sck_pin;
  uint32_t lrck_pin;
  uint32_t mck_pin;
  uint32_t sdout_pin;
  uint32_t sdin_pin;
  uint8_t irq_priority;

  I2CSlavePort *codec;

  const BoardPowerOps *power_ops;

  uint32_t samplerate;
} AudioDevice;
