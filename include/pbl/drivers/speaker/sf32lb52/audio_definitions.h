/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"
#include <pbl/drivers/audio.h>
#include <pbl/util/circular_buffer.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_speaker_sf32lb52 SF32LB52 audio
 * @ingroup drivers_audio
 * @brief @ref drivers_audio implementation for the SF32LB52 audio codec (AUDCODEC) DAC.
 * @{
 */

/** @brief Size, in bytes, of each half of the DAC DMA buffer. */
#define CFG_AUDIO_PLAYBACK_PIPE_SIZE (1024)

/** @brief Length of the queue between audio_write() and DMA, in milliseconds. */
#define CIRCULAR_BUF_SIZE_MS (128)
/** @brief Length of the queue between audio_write() and DMA, in samples. */
#define CIRCULAR_BUF_SIZE_SAMPLES ((MIC_SAMPLE_RATE * CIRCULAR_BUF_SIZE_MS) / 1000)
/** @brief Size of the queue between audio_write() and DMA, in bytes. */
#define CIRCULAR_BUF_SIZE_BYTES (CIRCULAR_BUF_SIZE_SAMPLES * sizeof(int16_t))

/** @cond INTERNAL_HIDDEN */
typedef enum AUDIO_PLL_STATE_TAG {
  AUDIO_PLL_CLOSED,
  AUDIO_PLL_OPEN,
  AUDIO_PLL_ENABLE,
} AUDIO_PLL_STATE;

typedef struct AudioState {
  AUDCODEC_HandleTypeDef audcodec;
  DMA_HandleTypeDef dac_dma_handle;
  AUDPRC_HandleTypeDef audprc;
  uint32_t slot_valid;
  uint8_t *queue_buf[HAL_AUDPRC_INSTANC_CNT];
  uint8_t *audec_queue_buf[HAL_AUDCODEC_INSTANC_CNT];
  AUDIO_PLL_STATE pll_state;
  uint32_t pll_samplerate;
  uint8_t tx_instanc;
  bool tx_rbf_enable;
  uint16_t tx_buffer_size;
  uint8_t *circ_buffer_storage;
  CircularBuffer circ_buffer;
  AudioTransCB trans_cb;
  AudioPlaybackCB playback_cb;
  void *playback_context;
  uint32_t playback_time;
  bool playback_started;
  bool running;
  /**
   * Set while a prv_audio_trans_bg refill callback is queued on the system
   * task; the DMA ISR must not enqueue another until it has run.
   */
  volatile bool callback_pending;
  uint8_t volume;
  /**
   * Raw (unaligned) pointer returned by kernel_malloc for the AUDCODEC DAC
   * DMA buffer. haudcodec->buf[] is bumped up to a cache-line boundary so
   * dcache_flush() of one half can't touch the other half's lines.
   */
  uint8_t *raw_dac_buffer;
#ifdef CONFIG_SPEAKER_SF32LB_DIAGNOSTICS
  volatile uint32_t diagnostic_refills;
  volatile uint32_t diagnostic_underrun_bytes;
  volatile uint32_t diagnostic_signal_samples;
  volatile uint32_t diagnostic_peak;
  uint32_t diagnostic_write_drops;
#endif
} AudioDeviceState;
/** @endcond */

/** @brief SF32LB52 audio device. */
typedef const struct AudioDevice {
  /** Driver runtime state. */
  AudioDeviceState *state;
  /** DAC DMA interrupt priority. */
  uint32_t irq_priority;
  /** AUDPRC DMA channel; not used by the driver. */
  DMA_Channel_TypeDef *audprc_dma_channel;
  /** AUDPRC DMA request; not used by the driver. */
  uint32_t audprc_dma_request;
  /** AUDPRC DMA interrupt; not used by the driver. */
  IRQn_Type audprc_dma_irq;
  /** AUDCODEC DAC DMA channel. */
  DMA_Channel_TypeDef *audec_dma_channel;
  /** AUDCODEC DAC DMA request. */
  uint32_t audec_dma_request;
  /** AUDCODEC DAC DMA interrupt. */
  IRQn_Type audec_dma_irq;
  /** Power amplifier enable output. */
  OutputConfig pa_ctrl;
  /** Board power hooks, or NULL. */
  const BoardPowerOps *power_ops;
  /** Sample width in bits; not used by the driver. */
  uint8_t data_format;
  /** Data mode; not used by the driver. */
  uint8_t data_mode;
  /** Sample rate in Hz. */
  uint32_t samplerate;
  /** Number of channels. */
  uint32_t channels;
} AudioDevice;

/**
 * @brief AUDCODEC DAC DMA interrupt handler.
 *
 * @param audio_device Audio device.
 */
extern void audec_dac0_dma_irq_handler(AudioDevice *audio_device);

/** @} */
