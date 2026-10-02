/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"
#include <pbl/drivers/audio.h>
#include "pbl/util/circular_buffer.h"

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_i2s.h"

/**
 * @defgroup drivers_speaker_nrf5 DA7212 on nRF5
 * @ingroup drivers_audio
 * @brief @ref drivers_audio implementation for a DA7212 codec fed by the nRF5 I2S peripheral.
 * @{
 */

/**
 * @brief Mono samples per I2S buffer.
 *
 * The stereo I2S buffer holds twice as many 16-bit samples; 512 mono samples are 32 ms at
 * 16 kHz.
 */
#define NRF5_AUDIO_I2S_BUF_SAMPLES_MONO 512
/** @brief Number of I2S buffers. */
#define NRF5_AUDIO_I2S_BUF_COUNT 2

/** @brief Size of the queue between audio_write() and DMA, in bytes (128 ms at 16 kHz). */
#define NRF5_AUDIO_CIRC_BUF_SIZE_BYTES 4096

/** @brief Free queue space, in bytes, at which the driver requests a refill. */
#define NRF5_AUDIO_REFILL_THRESHOLD_BYTES 1024

/** @cond INTERNAL_HIDDEN */
typedef struct AudioDeviceState {
  AudioTransCB trans_cb;
  bool is_running;
  bool callback_pending;
  uint8_t buf_idx;

  // Requested volume in percent, cached so it can be applied whenever the
  // codec is (re)powered.
  uint8_t volume;

  int16_t *i2s_bufs[NRF5_AUDIO_I2S_BUF_COUNT];

  uint8_t *circ_buffer_storage;
  CircularBuffer circ_buffer;
} AudioDeviceState;
/** @endcond */

/** @brief DA7212 audio device. */
typedef const struct AudioDevice {
  /** Driver runtime state. */
  AudioDeviceState *state;

  /** I2S instance. */
  nrfx_i2s_t i2s_instance;
  /** I2S SCK pin, to the codec BCLK. */
  uint32_t sck_pin;
  /** I2S LRCK pin, to the codec WCLK. */
  uint32_t lrck_pin;
  /** I2S MCK pin, to the codec MCLK. */
  uint32_t mck_pin;
  /** I2S SDOUT pin, to the codec DATA_IN. */
  uint32_t sdout_pin;
  /** I2S SDIN pin, or @c NRF_I2S_PIN_NOT_CONNECTED. */
  uint32_t sdin_pin;
  /** I2S interrupt priority. */
  uint8_t irq_priority;

  /** Codec control port. */
  I2CSlavePort *codec;

  /** Board power hooks, or NULL. */
  const BoardPowerOps *power_ops;

  /** Sample rate in Hz. */
  uint32_t samplerate;
} AudioDevice;

/** @} */
