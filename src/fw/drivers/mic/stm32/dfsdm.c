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

#include "dfsdm_definitions.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "kernel/util/sleep.h"
#include "kernel/util/stop.h"
#include "os/mutex.h"
#include "os/tick.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/profiler.h"
#include "util/circular_buffer.h"

#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include "FreeRTOS.h"
#include "semphr.h"

#define MAX_VOLUME (256)
#define LFSR_SEED (0x3AEF)

static bool prv_dma_handler(DMARequest *request, void *context, bool is_complete);

static void prv_enable_clocks(MicDevice *this) {
  // Enable the device clocks
  periph_config_acquire_lock();
  periph_config_enable(this->filter, this->rcc_apb_periph);
  periph_config_release_lock();
}

static void prv_disable_clocks(MicDevice *this) {
  // Disable DFSDM clock
  periph_config_acquire_lock();
  periph_config_disable(this->filter, this->rcc_apb_periph);
  periph_config_release_lock();
}

//! Configure GPIOs for DFSDM use
static void prv_enable_gpio(MicDevice *this, GPIOPuPd_TypeDef data_pupd) {
  gpio_af_init(&this->ck_gpio, GPIO_OType_PP,
               GPIO_Medium_Speed, GPIO_PuPd_NOPULL);

  // During normal operation it is probably more power-efficient to let
  // the data pin float (no pull) as no current would be wasted pulling
  // against the data signal. But the mic's data output pin goes Hi-Z
  // for half of each clock cycle, so power could be wasted on the
  // input if the signal voltage moves around too much while the mic
  // data output is Hi-Z.
  //
  // During self-test we want to enable a pull resistor so that we can
  // accurately detect the absence of the mic.
  gpio_af_init(&this->sd_gpio, GPIO_OType_PP,
               GPIO_Medium_Speed, data_pupd);
}

//! Configure GPIOs for lowest power consumption
static void prv_disable_gpio(MicDevice *this) {
  // Configure the clock pin as an output driving low so the microphone
  // won't see any unintentional clock edges which would wake it up
  // from sleep mode.
  gpio_af_configure_fixed_output(&this->ck_gpio, false);

  // Configure the data pin as an analog input, which is the lowest
  // power state it can be in. The mic's data pin goes into Hi-Z mode
  // when the mic is asleep, so the signal could float around and waste
  // power if the pin is configured as a digital input.
  gpio_af_configure_low_power(&this->sd_gpio);
}

static void prv_dfsdm_configure(MicDevice *this) {
  const uint32_t k_max_sinc4_osr = 255;

  PBL_ASSERTN(this->pdm_frequency > 0);
  PBL_ASSERTN(this->pdm_frequency % MIC_SAMPLE_RATE == 0);
  uint32_t oversampling_ratio = this->pdm_frequency / MIC_SAMPLE_RATE;
  uint32_t sinc_order = (oversampling_ratio <= k_max_sinc4_osr) ? DFSDM_SincOrder_Sinc4 :
      DFSDM_SincOrder_Sinc3;

  // Calculate the right shift needed to contain the final value within 24 bits
  int num_bits = ceil_log_two(oversampling_ratio);
  num_bits *= (sinc_order == DFSDM_SincOrder_Sinc4) ? 4 : 3;
  uint32_t right_shift = MAX(num_bits - 24, 0);

  RCC_ClocksTypeDef clocks;
  RCC_GetClocksFreq(&clocks);
  uint32_t prescaler = clocks.PCLK2_Frequency / this->pdm_frequency;

  // Disable the device before changing the config
  DFSDM_Command(DISABLE);
  DFSDM_ChannelCmd(this->channel, DISABLE);
  DFSDM_FilterCmd(this->filter, DISABLE);

  DFSDM_TransceiverInitTypeDef  DFSDM_InitStruct;
  DFSDM_TransceiverStructInit(&DFSDM_InitStruct);
  DFSDM_InitStruct.DFSDM_Interface = DFSDM_Interface_SPI_RisingEdge;
  DFSDM_InitStruct.DFSDM_Clock = DFSDM_Clock_Internal;
  DFSDM_InitStruct.DFSDM_Input = DFSDM_Input_External;
  DFSDM_InitStruct.DFSDM_Redirection = DFSDM_Redirection_Disabled;
  DFSDM_InitStruct.DFSDM_PackingMode = DFSDM_PackingMode_Standard;
  DFSDM_InitStruct.DFSDM_DataRightShift = right_shift;
  DFSDM_InitStruct.DFSDM_Offset = 0;
  DFSDM_InitStruct.DFSDM_CLKAbsenceDetector = DFSDM_CLKAbsenceDetector_Disable;
  DFSDM_InitStruct.DFSDM_ShortCircuitDetector = DFSDM_ShortCircuitDetector_Enable;

  DFSDM_TransceiverInit(this->channel, &DFSDM_InitStruct);

  DFSDM_FilterInitTypeDef DFSDM_FilterInitStruct;
  DFSDM_FilterStructInit(&DFSDM_FilterInitStruct);
  DFSDM_FilterInitStruct.DFSDM_SincOrder = sinc_order;
  DFSDM_FilterInitStruct.DFSDM_FilterOversamplingRatio = oversampling_ratio;
  DFSDM_FilterInitStruct.DFSDM_IntegratorOversamplingRatio = 1;
  DFSDM_FilterInit(this->filter, &DFSDM_FilterInitStruct);

  DFSDM_ConfigClkOutputSource(DFSDM_ClkOutSource_SysClock);

  DFSDM_ConfigClkOutputDivider(prescaler);
  DFSDM_SelectRegularChannel(this->filter, this->regular_channel);

  DFSDM_FastModeCmd(this->filter, ENABLE);
}

// must have initialized both the DFSDM and DMA
static void prv_dfsdm_enable(MicDevice *this) {
  // Enable DFSDM and DMA
  DFSDM_Command(ENABLE);
  DFSDM_ChannelCmd(this->channel, ENABLE);

  // Wait for microphone to power up
  psleep(this->power_on_delay_ms);

  // Configure DFSDM to use DMA and start the filter + DMA
  DFSDM_DMATransferConfig(this->filter, DFSDM_DMAConversionMode_Regular, ENABLE);
  DFSDM_FilterCmd(this->filter, ENABLE);
  DFSDM_RegularContinuousModeCmd(this->filter, ENABLE);
  DFSDM_StartSoftwareRegularConversion(this->filter);
  dma_request_start_circular(this->dma, this->state->in_buffer, (void *)&this->filter->FLTRDATAR,
                             sizeof(this->state->in_buffer), prv_dma_handler, (void *)this);
}

static void prv_dfsdm_disable(MicDevice *this) {
  // Disable DMA and DFSDM
  dma_request_stop(this->dma);
  DFSDM_ChannelCmd(this->channel, DISABLE);
  DFSDM_Command(DISABLE);

  prv_disable_clocks(this);
}

void mic_init(MicDevice *this) {
  PBL_ASSERTN(!this->state->initialized);

  this->state->main_pending = false;
  this->state->bg_pending = false;
  this->state->running = false;
  this->state->subscriber = (struct MicSubscriber){0};

  this->state->volume = this->default_volume;

  prv_disable_gpio(this);
  dma_request_init(this->dma);

  this->state->mic_mutex = mutex_create_recursive();
  this->state->initialized = true;
}

void mic_set_volume(MicDevice *this, uint16_t volume) {
  this->state->volume = MIN(MAX_VOLUME, volume);
}

bool mic_start(MicDevice *this, MicDataHandlerCB data_handler, void *context,
               int16_t *audio_buffer, size_t audio_buffer_len) {
  PBL_ASSERTN(this->state->initialized);
  mutex_lock_recursive(this->state->mic_mutex);

  bool success = false;

  if (this->state->running) {
    goto unlock;
  }

  circular_buffer_init(&this->state->circ_buffer,
                       this->state->circ_buf_store,
                       DFSDM_CIRC_BUFFER_SIZE);

  this->state->subscriber = (struct MicSubscriber) {
    .callback = data_handler,
    .buffer = audio_buffer,
    .context = context,
    .size = audio_buffer_len,
    .idx = 0
  };
  this->state->overflow_cnt = 0;
  this->state->bytes_received = 0;
  this->state->samples_to_discard = (MIC_SAMPLE_RATE * this->settling_delay_ms) / MS_PER_SECOND;

  this->state->hpf_y1 = 0;

  if (this->mic_power_state_fn) {
    this->mic_power_state_fn(true);
  }

  // Seed the LFSR random number generator
  this->state->prev_r = LFSR_SEED;

  prv_enable_gpio(this, GPIO_PuPd_NOPULL);
  prv_enable_clocks(this);
  prv_dfsdm_configure(this);
  prv_dfsdm_enable(this);

  DFSDM_RegularContinuousModeCmd(this->filter, ENABLE);
  DFSDM_StartSoftwareRegularConversion(this->filter);

  stop_mode_disable(InhibitorMic);
  this->state->running = true;
  success = true;

unlock:
  mutex_unlock_recursive(this->state->mic_mutex);

  return success;
}

void mic_stop(MicDevice *this) {
  mutex_lock_recursive(this->state->mic_mutex);

  if (!this->state->running) {
    goto unlock;
  }

  prv_dfsdm_disable(this);
  prv_disable_gpio(this);

  if (this->mic_power_state_fn) {
    this->mic_power_state_fn(false);
  }

  stop_mode_enable(InhibitorMic);
  this->state->running = false;

  PBL_LOG(LOG_LEVEL_DEBUG, "Stopped microphone, dropped samples: %"PRIu32" bytes received: %"PRIu32,
          this->state->overflow_cnt,
          this->state->bytes_received);

unlock:
  mutex_unlock_recursive(this->state->mic_mutex);
}

bool mic_is_running(MicDevice *this) {
  return this->state->running;
}

static void prv_dispatch_samples(void *context_ptr) {
  MicDevice *this = context_ptr;
  mutex_lock_recursive(this->state->mic_mutex);

  // if this->running is set to false (mic_stop is called)
  // while the loop is running, the remaining samples must be discarded.
  // If mic_stop is called from the subscriber callback, no more samples
  // must be read into the subscriber buffer (it is assumed to be invalid
  // memory at that point)
  while (this->state->running) {
    uint16_t size = circular_buffer_copy(
        &this->state->circ_buffer,
        (uint8_t *)&this->state->subscriber.buffer[this->state->subscriber.idx],
        ((this->state->subscriber.size - this->state->subscriber.idx) *
          sizeof(int16_t)));
    if (size == 0) {
      break;
    }

    // Only call the subscriber when the buffer is full. This takes away the
    // overhead of handling this in the subscriber module
    this->state->subscriber.idx += (size / sizeof(int16_t));
    if (this->state->subscriber.idx == this->state->subscriber.size) {
      this->state->subscriber.callback(this->state->subscriber.buffer,
                                       this->state->subscriber.idx,
                                       this->state->subscriber.context);
      this->state->subscriber.idx = 0;
    }

    // Make sure to maintain correct alignment when consuming bytes
    size -= size % sizeof(int16_t);
    circular_buffer_consume(&this->state->circ_buffer, size);
  }
  mutex_unlock_recursive(this->state->mic_mutex);
}

static void prv_dispatch_samples_main(void *context_ptr) {
  MicDevice *this = context_ptr;
  this->state->main_pending = false;
  prv_dispatch_samples(context_ptr);
}

static void prv_dispatch_samples_bg(void *context_ptr) {
  MicDevice *this = context_ptr;
  this->state->bg_pending = false;
  prv_dispatch_samples(context_ptr);
}

// Interrupt functions
////////////////////////////////////////////////////////////////////////////////////////////////

// Galois LFSR random number generator for dithering
static int16_t prv_get_lfsr(int16_t prev) {
  uint16_t lfsr = (uint16_t)prev;
  lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400);
  return (int16_t)lfsr;
}

static bool prv_dma_handler(DMARequest *request, void *context, bool is_complete) {
  MicDevice *this = context;
  bool should_context_switch = false;

  PROFILER_NODE_START(mic);

  const int32_t *dfsdm_buffer = this->state->in_buffer[is_complete ? 1 : 0];
  if (this->state->bytes_received == 0) {
    // Seed the filter state to prevent transient at the beginning of recording
    this->state->hpf_y1 = dfsdm_buffer[0] >> 8;
  }

  this->state->bytes_received += DFSDM_BUFFER_LENGTH * sizeof(*dfsdm_buffer);

  for (int i = 0; i < DFSDM_BUFFER_LENGTH; i++) {
    // move 24-bit value stored in upper 24-bits down to lower 24 bits
    int32_t sample = dfsdm_buffer[i] >> 8;

    // Single pole IIR filter to remove DC offset (cutoff frequency: 10Hz)
    // Filter coefficients pre-calculated
    const int64_t b1 = 32639; // b1 = exp(2 * PI * Fc) where Fc is 10/16000, scaled by 2^15
    const int64_t a0 = 129;   // a0 = 1 - b1, scaled by 2^15

    // Filter calculation
    int64_t y = a0 * (int64_t)sample + b1 * this->state->hpf_y1;
    // Scale down the value
    this->state->hpf_y1 = y >> 15;
    // HPF derived from low pass filter result
    sample -= this->state->hpf_y1;

    // Gain control - multiply the sample by up to MAX_VOLUME (256). This adds at most 8 bits to
    // the signal for full 32-bit resolution. Afterwards, add dither to reduce quantization noise,
    // then shift the signal down to fit it into 16 bits.
    // Note: we do not shift by the full 16 bits because, for normal speech, the received signal
    // is closer to 20/21 bits on bigboards.

    // Apply volume control
    sample *= this->state->volume;

    // Dither the sample by adding 2-LSB (post-shift) random TPDF (Triangular Probability Density
    // Function) noise. Create TPDF noise by summing 2 random numbers together. Use the previously
    // generated random number for computational efficiency (low pass filters the noise somewhat,
    // but the result is good). Shift the value
    int16_t r = prv_get_lfsr(this->state->prev_r);

    // divide by 2 to prevent overflows
    int16_t tpdf = (r / 2) + (this->state->prev_r / 2);

    this->state->prev_r = r;
    tpdf >>= this->final_right_shift - 1; // Shift to 2-LSB size relative to sample
    sample += tpdf;

    // Shift result to final bit depth and clip
    sample >>= this->final_right_shift;
    __SSAT(sample, 16); // saturate because we're clipping off the top of a 32-bit value

    if (this->state->samples_to_discard > 0) {
      this->state->samples_to_discard--;
    } else {
      if (!circular_buffer_write(&this->state->circ_buffer,
                                 (const uint8_t *) &sample,
                                 sizeof(int16_t))) {
        this->state->overflow_cnt++;
        break;
      }
    }
  }

  // Post a callback to KernelBG and KernelMain for faster servicing (check out PBL-40943 for more
  // details)
  bool main_switch_context = false;
  bool system_task_switch_context = false;
  if (!this->state->main_pending) {
    this->state->main_pending = true;
    PebbleEvent e = {
      .type = PEBBLE_CALLBACK_EVENT,
      .callback = {
        .callback = prv_dispatch_samples_main,
        .data = (void *)this
      }
    };
    main_switch_context = event_put_isr(&e);
  }

  if (!this->state->bg_pending) {
    this->state->bg_pending = true;
    system_task_add_callback_from_isr(prv_dispatch_samples_bg, (void *)this,
                                      &system_task_switch_context);
  }

  PROFILER_NODE_STOP(mic);
  return system_task_switch_context || main_switch_context;
}

extern MicDevice * const MIC;

bool mic_selftest(void) {
  stop_mode_disable(InhibitorMic);

  // Configure mic serial data pin with pull-down so that the GPIO does
  // not float if there is an open-circuit condition on the pin.
  prv_enable_gpio(MIC, GPIO_PuPd_DOWN);

  // Configure the DFSDM peripheral with short-circuit detection.
  prv_enable_clocks(MIC);
  // Set the short-circut threshold length to its maximum value.
  DFSDM_ConfigShortCircuitThreshold(DFSDM1_Channel2, 255);
  prv_dfsdm_configure(MIC);

  // Start DFSDM conversion without DMA. Throw out the samples; we only
  // care about the short-circuit detection.
  DFSDM_Command(ENABLE);
  DFSDM_ChannelCmd(DFSDM1_Channel2, ENABLE);
  DFSDM_FilterCmd((DFSDM_TypeDef *) DFSDM1_Filter0_BASE, ENABLE);
  DFSDM_RegularContinuousModeCmd((DFSDM_TypeDef *) DFSDM1_Filter0_BASE, ENABLE);
  DFSDM_StartSoftwareRegularConversion((DFSDM_TypeDef *) DFSDM1_Filter0_BASE);

  // Wait until the microphone wakes up. The datasheet max wakeup time
  // for the two microphones we may use on Silk is around 30 ms. Add in
  // some extra margin in case we make a running change and use another
  // microphone that is even slower to wake up.
  psleep(50);

  // Run the actual test.
  DFSDM_ClearShortCircuitFlag(DFSDM_CLEARF_SCD_Channel2);
  psleep(10);
  const bool test_pass = DFSDM_GetShortCircuitFlagStatus(DFSDM_IT_SCD_Channel2) != SET;

  prv_dfsdm_disable(MIC);
  prv_disable_gpio(MIC);
  stop_mode_enable(InhibitorMic);

  return test_pass;
}
