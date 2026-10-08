/* SPDX-FileCopyrightText: 2025 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <board/board.h>
#include <pbl/drivers/mic.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/circular_buffer.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_mic_nrf5 nRF5 PDM microphone
 * @ingroup drivers_mic
 * @brief @ref drivers_mic implementation for the nRF5 PDM peripheral.
 * @{
 */

/** @brief Samples per PDM DMA buffer. */
#define PDM_BUFFER_SIZE_SAMPLES (320)
/** @brief Number of PDM DMA buffers. */
#define PDM_BUFFER_COUNT (2)
/** @brief Default PDM gain. */
#define PDM_GAIN_DEFAULT (NRF_PDM_GAIN_DEFAULT)

/** @brief Preferred length of the queue between DMA and the data handler, in milliseconds. */
#define CIRCULAR_BUF_SIZE_MS (320)
/** @brief Preferred queue length, in samples. */
#define CIRCULAR_BUF_SIZE_SAMPLES ((MIC_SAMPLE_RATE * CIRCULAR_BUF_SIZE_MS) / 1000)
/** @brief Preferred queue size, in bytes. */
#define CIRCULAR_BUF_SIZE_BYTES (CIRCULAR_BUF_SIZE_SAMPLES * sizeof(int16_t))

/**
 * @brief Minimum queue length, in milliseconds, accepted when the heap is too fragmented for the
 * preferred one.
 *
 * Transcription quality degrades noticeably below it.
 */
#define CIRCULAR_BUF_MIN_SIZE_MS (128)
/** @brief Minimum queue length, in samples. */
#define CIRCULAR_BUF_MIN_SIZE_SAMPLES ((MIC_SAMPLE_RATE * CIRCULAR_BUF_MIN_SIZE_MS) / 1000)
/** @brief Minimum queue size, in bytes. */
#define CIRCULAR_BUF_MIN_SIZE_BYTES (CIRCULAR_BUF_MIN_SIZE_SAMPLES * sizeof(int16_t))

/**
 * @brief Queue shrink per allocation retry, in milliseconds.
 *
 * Gives about 7 attempts between the preferred and the minimum length.
 */
#define CIRCULAR_BUF_STEP_MS (32)
/** @brief Queue shrink per allocation retry, in samples. */
#define CIRCULAR_BUF_STEP_SAMPLES ((MIC_SAMPLE_RATE * CIRCULAR_BUF_STEP_MS) / 1000)
/** @brief Queue shrink per allocation retry, in bytes. */
#define CIRCULAR_BUF_STEP_BYTES (CIRCULAR_BUF_STEP_SAMPLES * sizeof(int16_t))

/** @cond INTERNAL_HIDDEN */
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
  uint16_t circ_buffer_size;

  // State management
  struct pbl_mutex mutex;
  bool is_running;
  bool is_initialized;
  bool main_pending;
} MicDeviceState;
/** @endcond */

/** @brief nRF5 PDM microphone device. */
typedef const struct MicDevice {
  /** Driver runtime state. */
  MicDeviceState *state;

  /** PDM instance. */
  const nrfx_pdm_t pdm_instance;
  /** PDM clock pin. */
  uint32_t clk_pin;
  /** PDM data pin. */
  uint32_t data_pin;
  /** Number of channels, 1 or 2. */
  uint32_t channels;
} MicDevice;

/** @} */
