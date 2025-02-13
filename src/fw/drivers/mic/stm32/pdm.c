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

#include "drivers/mic.h"

#include "board/board.h"
#include "drivers/accessory.h"
#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/stop.h"
#include "mfg/mfg_mode/mfg_factory_mode.h"
#include "services/common/system_task.h"
#if RECOVERY_FW
#include "services/prf/accessory/accessory_manager.h"
#else
#include "services/normal/accessory/accessory_manager.h"
#endif
#include "system/logging.h"
#include "os/mutex.h"
#include "system/passert.h"
#include "system/profiler.h"
#include "util/circular_buffer.h"
#include "util/legacy_checksum.h"
#include "util/math.h"
#include "util/net.h"
#include "util/size.h"

#define STM32F4_COMPATIBLE
#include <mcu.h>

#include "vendor/ST-libPDM/pdm_filter.h"

#define DECIMATION_FACTOR         (64)
#define IN_BUF_BATCH_SIZE         (8)
#define CIRCULAR_BUF_BATCH_SIZE   (10)

// This gives two 1K byte buffers for DMA, which will each fill in about 1/64s
#define IN_BUFFER_LENGTH    (DECIMATION_FACTOR * IN_BUF_BATCH_SIZE)

static uint16_t s_in_buffer[2][IN_BUFFER_LENGTH];

static uint8_t s_circ_buf_store[(MIC_SAMPLE_RATE / 1000) * CIRCULAR_BUF_BATCH_SIZE * 4
                                * sizeof(uint16_t)];

static PDMFilter_InitStruct s_pdm_filter;

static uint16_t s_volume;

static bool s_running = false;

static bool s_main_pending = false;
static bool s_bg_pending = false;
static int s_overflow_cnt;
static bool s_initialized = false;
static uint8_t s_discarded;
static CircularBuffer s_circ_buffer;

// A mutex is needed to protect against a race condition between mic_stop and the dispatch routine
// potentially resulting in the deallocation of the subscriber module's receive buffer while the
// dispatch routine is still running.
static PebbleRecursiveMutex *s_mic_mutex;

static struct Subscriber {
  MicDataHandlerCB callback;
  int16_t *buffer;
  void *context;
  size_t size;
  size_t idx;
} s_subscriber;

static bool prv_dma_handler(DMARequest *request, void *context, bool is_complete);

//! Initialize power management for the microphone. Note that different boards have different
//! ways of configuring power to the mic.
static void prv_mic_power_init(void) {
  if (BOARD_CONFIG.mic_config.mic_gpio_power.gpio) {
    gpio_use(BOARD_CONFIG.mic_config.mic_gpio_power.gpio);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_Pin = BOARD_CONFIG.mic_config.mic_gpio_power.gpio_pin;
    GPIO_Init(GPIOF, &GPIO_InitStructure);

    gpio_output_set(&BOARD_CONFIG.mic_config.mic_gpio_power, false);

    gpio_release(BOARD_CONFIG.mic_config.mic_gpio_power.gpio);
  }
}

static void prv_i2s_gpio_init(void) {
  // Enable the SPI clock
  periph_config_acquire_lock();

  // [AS] TODO: If I2S is moved to SPI1, this RCC function needs to be abstracted to board.h
  PBL_ASSERTN(BOARD_CONFIG.mic_config.spi_clock_ctrl != RCC_APB2Periph_SPI1);
  periph_config_enable(BOARD_CONFIG.mic_config.spi,
                       BOARD_CONFIG.mic_config.spi_clock_ctrl);
  periph_config_release_lock();

  gpio_use(BOARD_CONFIG.mic_config.i2s_ck.gpio);
  gpio_use(BOARD_CONFIG.mic_config.i2s_sd.gpio);

  // Configure pins as SPI/I2S pins
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;

  // I2S CK
  GPIO_InitStructure.GPIO_Pin = BOARD_CONFIG.mic_config.i2s_ck.gpio_pin;
  GPIO_Init(BOARD_CONFIG.mic_config.i2s_ck.gpio, &GPIO_InitStructure);
  GPIO_PinAFConfig(BOARD_CONFIG.mic_config.i2s_ck.gpio,
      BOARD_CONFIG.mic_config.i2s_ck.gpio_pin_source, BOARD_CONFIG.mic_config.i2s_ck.gpio_af);

  // I2S SD
  GPIO_InitStructure.GPIO_Pin =  BOARD_CONFIG.mic_config.i2s_sd.gpio_pin;
  GPIO_Init(BOARD_CONFIG.mic_config.i2s_sd.gpio, &GPIO_InitStructure);
  GPIO_PinAFConfig(BOARD_CONFIG.mic_config.i2s_sd.gpio,
      BOARD_CONFIG.mic_config.i2s_sd.gpio_pin_source, BOARD_CONFIG.mic_config.i2s_sd.gpio_af);

  gpio_release(BOARD_CONFIG.mic_config.i2s_ck.gpio);
  gpio_release(BOARD_CONFIG.mic_config.i2s_sd.gpio);

  // I2S configuration
  SPI_I2S_DeInit(BOARD_CONFIG.mic_config.spi);
  I2S_InitTypeDef  I2S_InitStructure;
  I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq_32k;
  I2S_InitStructure.I2S_Standard = I2S_Standard_LSB;
  I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
  I2S_InitStructure.I2S_CPOL = I2S_CPOL_High;
  I2S_InitStructure.I2S_Mode = I2S_Mode_MasterRx;
  I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
  I2S_Init(BOARD_CONFIG.mic_config.spi, &I2S_InitStructure);

  periph_config_acquire_lock();
  periph_config_disable(BOARD_CONFIG.mic_config.spi,
                        BOARD_CONFIG.mic_config.spi_clock_ctrl);
  periph_config_release_lock();
}

static void prv_mic_power_enable(void) {
  if (BOARD_CONFIG.mic_config.mic_gpio_power.gpio) {
    gpio_output_set(&BOARD_CONFIG.mic_config.mic_gpio_power, true);
  } else {
    set_ldo3_power_state(true);
  }
}

static void prv_mic_power_disable(void) {
  if (BOARD_CONFIG.mic_config.mic_gpio_power.gpio) {
    gpio_output_set(&BOARD_CONFIG.mic_config.mic_gpio_power, false);
  } else {
    set_ldo3_power_state(false);
  }
}

void mic_init(MicDevice *this) {
  PBL_ASSERTN(!s_initialized);

  s_main_pending = false;
  s_bg_pending = false;
  s_running = false;
  s_subscriber = (struct Subscriber){0};

  s_volume = BOARD_CONFIG.mic_config.gain;

  s_pdm_filter.Fs = MIC_SAMPLE_RATE;
  s_pdm_filter.LP_HZ = 8000;
  s_pdm_filter.HP_HZ = 10;
  s_pdm_filter.Out_MicChannels = 1;
  s_pdm_filter.In_MicChannels = 1;

  dma_request_init(MIC_I2S_RX_DMA);

  prv_i2s_gpio_init();
  prv_mic_power_init();

  s_mic_mutex = mutex_create_recursive();

  s_initialized = true;
}

void mic_set_volume(MicDevice *this, uint16_t volume) {
  s_volume = volume;
}

bool mic_start(MicDevice *mic, MicDataHandlerCB data_handler, void *context,
               int16_t *audio_buffer, size_t audio_buffer_len) {
  PBL_ASSERTN(s_initialized);
  mutex_lock_recursive(s_mic_mutex);

  bool success = false;

  if (s_running) {
    goto unlock;
  }

  prv_mic_power_enable();

  circular_buffer_init(&s_circ_buffer, s_circ_buf_store, sizeof(s_circ_buf_store));
  s_subscriber = (struct Subscriber) {
    .callback = data_handler,
    .buffer = audio_buffer,
    .context = context,
    .size = audio_buffer_len,
    .idx = 0
  };
  s_overflow_cnt = 0;
  s_discarded = 0;

  // The filter library checks that the CRC is present on the platform. Yay DRM
  periph_config_enable(CRC, RCC_AHB1Periph_CRC);
  CRC_ResetDR();
  PDM_Filter_Init(&s_pdm_filter);
  periph_config_disable(CRC, RCC_AHB1Periph_CRC);

  //Enable I2S PLL
  RCC_PLLI2SCmd(ENABLE);
  while (RCC_GetFlagStatus(RCC_FLAG_PLLI2SRDY) == RESET) {}

  // Enable I2S clock
  periph_config_acquire_lock();
  periph_config_enable(BOARD_CONFIG.mic_config.spi, BOARD_CONFIG.mic_config.spi_clock_ctrl);
  periph_config_release_lock();

  // Configure I2S to use DMA
  SPI_I2S_DMACmd(BOARD_CONFIG.mic_config.spi, SPI_I2S_DMAReq_Rx, ENABLE);

  // Enable I2S
  I2S_Cmd(BOARD_CONFIG.mic_config.spi, ENABLE);

  // DMA config - use single buffer circular mode. Pointer to buffer is a 2-D array for ease of
  // access
  void *periph_addr = (void *)&BOARD_CONFIG.mic_config.spi->DR;
  stop_mode_disable(InhibitorMic);
  dma_request_start_circular(MIC_I2S_RX_DMA, s_in_buffer, periph_addr, sizeof(s_in_buffer),
                             prv_dma_handler, NULL);

  s_running = true;
  success = true;

unlock:
  mutex_unlock_recursive(s_mic_mutex);

  return success;
}

void mic_stop(MicDevice *this) {
  mutex_lock_recursive(s_mic_mutex);

  if (!s_running) {
    goto unlock;
  }

  // Disable DMA and I2S
  dma_request_stop(MIC_I2S_RX_DMA);
  I2S_Cmd(BOARD_CONFIG.mic_config.spi, DISABLE);

  // Disable I2S clock
  periph_config_acquire_lock();
  periph_config_disable(BOARD_CONFIG.mic_config.spi,
                        BOARD_CONFIG.mic_config.spi_clock_ctrl);
  periph_config_release_lock();

  // Disable I2S PLL
  RCC_PLLI2SCmd(DISABLE);

  prv_mic_power_disable();
  stop_mode_enable(InhibitorMic);
  s_running = false;

  PBL_LOG(LOG_LEVEL_DEBUG, "Stopped microphone, dropped samples: %d", s_overflow_cnt);

unlock:
  mutex_unlock_recursive(s_mic_mutex);
}

bool mic_is_running(MicDevice *this) {
  return s_running;
}

static void prv_dispatch_samples_common(void) {
  mutex_lock_recursive(s_mic_mutex);

  // if s_running is set to false (mic_stop is called) while the loop is running, the
  // remaining samples must be discarded. If mic_stop is called from the subscriber callback,
  // no more samples must be read into the subscriber buffer (it is assumed to be invalid memory at
  // that point)
  while (s_running) {
    uint16_t size = circular_buffer_copy(&s_circ_buffer,
        (uint8_t *) &s_subscriber.buffer[s_subscriber.idx],
        ((s_subscriber.size - s_subscriber.idx) * sizeof(int16_t)));

    if (size == 0) {
      break;
    }

    // Only call the subscriber when the buffer is full. This takes away the
    // overhead of handling this in the subscriber module
    s_subscriber.idx += (size / sizeof(int16_t));
    if (s_subscriber.idx == s_subscriber.size) {
      s_subscriber.callback(s_subscriber.buffer, s_subscriber.idx, s_subscriber.context);
      s_subscriber.idx = 0;
    }

    // Make sure to maintain correct alignment when consuming bytes
    size -= size % sizeof(int16_t);
    circular_buffer_consume(&s_circ_buffer, size);
  }

  mutex_unlock_recursive(s_mic_mutex);
}

static void prv_dispatch_samples_main(void* data) {
  // Setting this to false before we process the data means that we'll have at most 2 callbacks on
  // the queue. Putting it after the processing step means that there is a possible race
  // condition with setting and clearing the flag that could result in overflow
  s_main_pending = false;

  prv_dispatch_samples_common();
}

static void prv_dispatch_samples_bg(void* data) {
  // Setting this to false before we process the data means that we'll have at most 2 callbacks on
  // the queue. Putting it after the processing step means that there is a possible race
  // condition with setting and clearing the flag that could result in overflow
  s_bg_pending = false;

  prv_dispatch_samples_common();
}


// Interrupt functions
////////////////////////////////////////////////////////////////////////////////////////////////

static bool prv_dma_handler(DMARequest *request, void *context, bool is_complete) {
  const uint8_t MS_TO_SETTLE = 100;
  PROFILER_NODE_START(mic);

  uint16_t *pdm_buffer = s_in_buffer[is_complete ? 1 : 0];
  // byte endianness needs to be swapped for the filter library
  for (size_t i = 0; i < ARRAY_LENGTH(s_in_buffer[0]); i++) {
    pdm_buffer[i] = htons(pdm_buffer[i]);
  }

  bool overflow = false;
  uint16_t pcm16_buffer[MIC_SAMPLE_RATE / 1000]; // Store enough for 1 millisecond of data
  uint16_t *pdm_end = pdm_buffer + ARRAY_LENGTH(s_in_buffer[0]);
  while (pdm_buffer < pdm_end) {
    // Process one millisecond of data per call
    PDM_Filter_64_LSB((uint8_t *)pdm_buffer, pcm16_buffer, s_volume,
        (PDMFilter_InitStruct *)&s_pdm_filter);
    pdm_buffer += DECIMATION_FACTOR;

    // while the filter is settling discard samples (about 100 ms)
    // each iteration of this loop writes 1 ms of data to the buffer
    if (s_discarded < MS_TO_SETTLE) {
      s_discarded++;
    } else if (!circular_buffer_write(&s_circ_buffer, (const uint8_t *) pcm16_buffer,
                                      sizeof(pcm16_buffer))) {
      overflow = true;
    }
  }

  // Only count one overflow per interrupt
  if (overflow) {
    s_overflow_cnt++;
  }

  // We post an event to both KernelMain and KernelBG. It is critical that the microphone
  // data be processed quickly so that we don't encounter buffer overruns. Occasionally
  // KernelMain can be busy for long periods of time (24ms to do a display DMA for example)
  // so we also post to KernelBG. Whichever happens to get to the event first will process the
  // buffer and the other task will quickly find that the buffer has already been emptied.
  bool main_switch_context = false;
  bool system_task_switch_context = false;
  if (!s_main_pending) {
    // Only post a callback event if one is not already pending
    PebbleEvent e = {
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {
        .callback = prv_dispatch_samples_main,
        .data = NULL
      }
    };
    s_main_pending = true;
    main_switch_context = event_put_isr(&e);
  }

  if (!s_bg_pending) {
    // Only post a callback event if one is not already pending
    s_bg_pending = true;
    system_task_add_callback_from_isr(prv_dispatch_samples_bg, NULL,
                                      &system_task_switch_context);
  }

  PROFILER_NODE_STOP(mic);
  return main_switch_context || system_task_switch_context;
}
