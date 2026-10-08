/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <board/board.h>
#include <pbl/drivers/mic.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/circular_buffer.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_mic_sf32lb52 SF32LB52 PDM microphone
 * @ingroup drivers_mic
 * @brief @ref drivers_mic implementation for the SF32LB52 PDM peripheral.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct MicState {
  uint8_t *circ_buffer_storage;
  CircularBuffer circ_buffer;
  DMA_HandleTypeDef hdma;
  /**
   * Raw (unaligned) pointer returned by kernel_malloc for the PDM DMA buffer.
   * hpdm->pRxBuffPtr is bumped up to a cache-line boundary so the CPU can
   * invalidate it without clobbering adjacent dirty data.
   */
  uint8_t *raw_dma_buffer;

  // User interface
  MicDataHandlerCB data_handler;
  MicDataReadyCB ready_handler;
  void *handler_context;
  int16_t *audio_buffer;
  size_t audio_buffer_len;

  bool is_initialized;
  bool is_running;
  bool main_pending;
  bool bg_pending;
  uint16_t volume;
  uint8_t channels;
  uint32_t capture_epoch;
  uint32_t timed_samples;
  uint32_t frame_time;
  bool frame_time_valid;
  uint32_t capture_bytes;
  uint32_t dispatched_bytes;
  uint32_t dropped_bytes;
  uint16_t peak_backlog;

  // A mutex is needed to protect against a race condition between
  // mic_stop and the dispatch routine potentially resulting in the
  // deallocation of the subscriber module's receive buffer while the
  // dispatch routine is still running.
  struct pbl_mutex mutex;
  PDM_HandleTypeDef *hpdm;
} MicDeviceState;
/** @endcond */

/** @brief SF32LB52 PDM microphone device. */
typedef const struct MicDevice {
  /** Driver runtime state. */
  MicDeviceState *state;
  /** PDM instance. */
  PDM_TypeDef *pdm_instance;
  /** PDM interrupt. */
  IRQn_Type pdm_irq;
  /** PDM DMA interrupt. */
  IRQn_Type pdm_dma_irq;
  /** PDM clock pin. */
  Pinmux clk_gpio;
  /** PDM data pin. */
  Pinmux data_gpio;
  /** Number of channels, 1 or 2. */
  uint32_t channels;
  /** PDM sample rate in Hz. */
  uint32_t sample_rate;
  /** PDM channel depth in bits. */
  uint32_t channel_depth;
  /** Default volume scalar (max 128); not used by the driver. */
  uint16_t default_volume;
} MicDevice;

/**
 * @brief PDM interrupt handler.
 *
 * @param this Microphone device.
 */
extern void pdm1_data_handler(MicDevice *this);

/**
 * @brief PDM DMA interrupt handler.
 *
 * @param this Microphone device.
 */
extern void pdm1_l_dma_handler(MicDevice *this);

/** @} */
