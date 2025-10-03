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

#include "sharp_ls013b7dh01.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "applib/graphics/gtypes.h"
#include "board/board.h"
#include "drivers/gpio.h"
#include "kernel/events.h"
#include "kernel/util/stop.h"
#include "os/mutex.h"
#include "system/passert.h"
#include <hal/nrf_gpio.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_rtc.h>
#include <nrfx_gppi.h>
#include <nrfx_spim.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define DISP_MODE_WRITE 0x01U
#define DISP_MODE_CLEAR 0x04U

static uint8_t s_buf[2 + ((DISP_LINE_BYTES + 2) * PBL_DISPLAY_HEIGHT)];
static bool s_updating;
static UpdateCompleteCallback s_uccb;
static SemaphoreHandle_t s_sem;

static void prv_extcomin_init(void) {
  nrfx_err_t err;
  const NrfLowPowerPWM *extcomin = &BOARD_CONFIG_DISPLAY.extcomin;
  uint32_t evt_addr, task_addr;
  uint8_t ppi_ch[2];

  nrf_gpiote_te_default(extcomin->gpiote, extcomin->gpiote_ch);

  nrf_gpio_pin_write(extcomin->psel, 0);
  nrf_gpio_cfg_output(extcomin->psel);

  // RTC: CC0 is the period end, CC1 is the pulse end
  nrf_rtc_task_trigger(extcomin->rtc, NRF_RTC_TASK_STOP);
  nrf_rtc_event_clear(extcomin->rtc, nrf_rtc_compare_event_get(0));
  nrf_rtc_event_clear(extcomin->rtc, nrf_rtc_compare_event_get(1));
  nrf_rtc_task_trigger(extcomin->rtc, NRF_RTC_TASK_CLEAR);
  nrf_rtc_prescaler_set(extcomin->rtc, NRF_RTC_FREQ_TO_PRESCALER(32768));
  nrf_rtc_event_enable(extcomin->rtc, (NRF_RTC_INT_COMPARE0_MASK | NRF_RTC_INT_COMPARE1_MASK));
  nrf_rtc_cc_set(extcomin->rtc, 0, (32768 * extcomin->period_us) / 1000000 - 1);
  nrf_rtc_cc_set(extcomin->rtc, 1, (32768 * extcomin->pulse_us) / 1000000 - 1);

  nrf_gpiote_task_configure(extcomin->gpiote, extcomin->gpiote_ch, extcomin->psel,
                            NRF_GPIOTE_POLARITY_NONE, NRF_GPIOTE_INITIAL_VALUE_LOW);
  nrf_gpiote_task_enable(extcomin->gpiote, extcomin->gpiote_ch);

  err = nrfx_gppi_channel_alloc(&ppi_ch[0]);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  err = nrfx_gppi_channel_alloc(&ppi_ch[1]);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  // Period end (CC0) sets GPIO, clears RTC
  evt_addr = nrf_rtc_event_address_get(extcomin->rtc, nrf_rtc_compare_event_get(0));
  task_addr =
      nrf_gpiote_task_address_get(extcomin->gpiote, nrf_gpiote_set_task_get(extcomin->gpiote_ch));
  nrfx_gppi_channel_endpoints_setup(ppi_ch[0], evt_addr, task_addr);

  task_addr = nrf_rtc_event_address_get(extcomin->rtc, NRF_RTC_TASK_CLEAR);
  nrfx_gppi_fork_endpoint_setup(ppi_ch[0], task_addr);

  // Pulse end (CC1) clears GPIO
  evt_addr = nrf_rtc_event_address_get(extcomin->rtc, nrf_rtc_compare_event_get(1));
  task_addr =
      nrf_gpiote_task_address_get(extcomin->gpiote, nrf_gpiote_clr_task_get(extcomin->gpiote_ch));
  nrfx_gppi_channel_endpoints_setup(ppi_ch[1], evt_addr, task_addr);

  nrfx_gppi_channels_enable((1UL << ppi_ch[0]) | (1UL << ppi_ch[1]));

  nrf_rtc_task_trigger(extcomin->rtc, NRF_RTC_TASK_START);
}

static inline void prv_enable_spim(void) {
  nrf_spim_enable(BOARD_CONFIG_DISPLAY.spi.p_reg);
}

static inline void prv_disable_spim(void) {
  nrf_spim_disable(BOARD_CONFIG_DISPLAY.spi.p_reg);

  // Workaround for nRF52840 anomaly 195
  if (BOARD_CONFIG_DISPLAY.spi.p_reg == NRF_SPIM3) {
    *(volatile uint32_t *)0x4002F004 = 1;
  }
}

static inline void prv_enable_chip_select(void) {
  gpio_output_set(&BOARD_CONFIG_DISPLAY.cs, true);
}

static inline void prv_disable_chip_select(void) {
  gpio_output_set(&BOARD_CONFIG_DISPLAY.cs, false);
}

static void prv_terminate_transfer(void *data) {
  s_updating = false;

  prv_disable_chip_select();
  prv_disable_spim();

  s_uccb();
}

static void prv_spim_evt_handler(nrfx_spim_evt_t const *evt, void *ctx) {
  portBASE_TYPE woken = pdFALSE;

  if (s_updating) {
    PebbleEvent e = {
        .type = PEBBLE_CALLBACK_EVENT,
        .callback =
            {
                .callback = prv_terminate_transfer,
            },
    };

    woken = event_put_isr(&e) ? pdTRUE : pdFALSE;
  } else {
    xSemaphoreGiveFromISR(s_sem, &woken);
  }

  portEND_SWITCHING_ISR(woken);
}

void display_init(void) {
  nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG(
      BOARD_CONFIG_DISPLAY.clk.gpio_pin, BOARD_CONFIG_DISPLAY.mosi.gpio_pin,
      NRF_SPIM_PIN_NOT_CONNECTED, NRF_SPIM_PIN_NOT_CONNECTED);
  config.frequency = NRFX_MHZ_TO_HZ(1);
  config.bit_order = NRF_SPIM_BIT_ORDER_LSB_FIRST;

  nrfx_err_t err = nrfx_spim_init(&BOARD_CONFIG_DISPLAY.spi, &config, prv_spim_evt_handler, NULL);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  gpio_output_init(&BOARD_CONFIG_DISPLAY.cs, GPIO_OType_PP, GPIO_Speed_50MHz);

  gpio_output_init(&BOARD_CONFIG_DISPLAY.on_ctrl, BOARD_CONFIG_DISPLAY.on_ctrl_otype,
                   GPIO_Speed_50MHz);
  gpio_output_set(&BOARD_CONFIG_DISPLAY.on_ctrl, true);

  prv_extcomin_init();

  s_sem = xSemaphoreCreateBinary();
}

void display_clear(void) {
  uint8_t buf[] = {DISP_MODE_CLEAR, 0x00};
  nrfx_spim_xfer_desc_t desc = {.p_tx_buffer = buf, .tx_length = sizeof(buf)};

  PBL_ASSERTN(!s_updating);

  prv_enable_spim();
  prv_enable_chip_select();

  nrfx_err_t err = nrfx_spim_xfer(&BOARD_CONFIG_DISPLAY.spi, &desc, 0);
  PBL_ASSERTN(err == NRFX_SUCCESS);
  xSemaphoreTake(s_sem, portMAX_DELAY);

  prv_disable_chip_select();
  prv_disable_spim();
}

void display_set_enabled(bool enabled) {
  gpio_output_set(&BOARD_CONFIG_DISPLAY.on_ctrl, enabled);
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  DisplayRow row;
  uint8_t *pbuf = s_buf;
  nrfx_spim_xfer_desc_t desc = {.p_tx_buffer = pbuf};

  PBL_ASSERTN(!s_updating);

  // write command (write)
  *pbuf++ = DISP_MODE_WRITE;
  desc.tx_length++;

  while (nrcb(&row)) {
    // write row address, data and trailing dummy
    *pbuf++ = row.address + 1;
    memcpy(pbuf, row.data, DISP_LINE_BYTES);
    pbuf += DISP_LINE_BYTES;
    *pbuf++ = 0x00;

    desc.tx_length += DISP_LINE_BYTES + 2;
  }

  // write last trailing dummy
  *pbuf++ = 0x00;
  desc.tx_length++;

  prv_enable_spim();
  prv_enable_chip_select();

  s_uccb = uccb;
  s_updating = true;

  nrfx_err_t err = nrfx_spim_xfer(&BOARD_CONFIG_DISPLAY.spi, &desc, 0);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

bool display_update_in_progress(void) {
  return s_updating;
}

/* stubs */

uint32_t display_baud_rate_change(uint32_t new_frequency_hz) {
  return new_frequency_hz;
}

void display_pulse_vcom(void) {}

void display_show_splash_screen(void) {}

void display_set_offset(GPoint offset) {}

GPoint display_get_offset(void) {
  return GPointZero;
}
