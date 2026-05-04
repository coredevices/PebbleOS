/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "display_jdi.h"

#include "board/board.h"
#include "board/display.h"
#include "drivers/display/display.h"
#include "drivers/gpio.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/delay.h"
#include "kernel/util/stop.h"
#include "os/mutex.h"
#include "system/logging.h"
#include "system/passert.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "bf0_hal.h"
#include "bf0_hal_lcdc.h"
#include "bf0_hal_lptim.h"
#include "bf0_hal_rtc.h"

#define POWER_SEQ_DELAY_TIME_US  11000
#define POWER_RESET_CYCLE_DELAY_TIME_US 500000

// Pointer to the compositor's framebuffer - we convert in-place to save 44KB RAM
static uint8_t *s_framebuffer;
static uint16_t s_update_y0;
static uint16_t s_update_y1;
static bool s_initialized;
static bool s_updating;
static UpdateCompleteCallback s_uccb;
static SemaphoreHandle_t s_sem;

#if DISPLAY_ORIENTATION_ROTATED_180
static bool s_rotated_180 = true;
#else
static bool s_rotated_180 = false;
#endif

static void prv_power_cycle(void){
  OutputConfig cfg = {
    .gpio = hwp_gpio1,
    .active_high = true,
  };

  // This will disable all JDI pull-ups/downs so that VLCD can fully turn off,
  // allowing for a clean power cycle.

  cfg.gpio_pin = DISPLAY->pinmux.b1.pad - PAD_PA00;
  gpio_output_init(&cfg, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&cfg, false);

  cfg.gpio_pin = DISPLAY->pinmux.vck.pad - PAD_PA00;
  gpio_output_init(&cfg, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&cfg, false);

  cfg.gpio_pin = DISPLAY->pinmux.xrst.pad - PAD_PA00;
  gpio_output_init(&cfg, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&cfg, false);

  cfg.gpio_pin = DISPLAY->pinmux.hck.pad - PAD_PA00;
  gpio_output_init(&cfg, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&cfg, false);

  cfg.gpio_pin = DISPLAY->pinmux.r2.pad - PAD_PA00;
  gpio_output_init(&cfg, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&cfg, false);

  gpio_output_set(&DISPLAY->vddp, false);
  gpio_output_set(&DISPLAY->vlcd, false);

  delay_us(POWER_RESET_CYCLE_DELAY_TIME_US);
}

static void prv_display_on() {
  gpio_output_set(&DISPLAY->vlcd, true);
  delay_us(POWER_SEQ_DELAY_TIME_US);
  gpio_output_set(&DISPLAY->vddp, true);
  delay_us(POWER_SEQ_DELAY_TIME_US);

  LPTIM_TypeDef *lptim = DISPLAY->vcom.lptim;

  lptim->CFGR |= LPTIM_INTCLOCKSOURCE_LPCLOCK;
  lptim->ARR = RC10K_FREQ / DISPLAY->vcom.freq_hz;
  lptim->CMP = lptim->ARR / 2;
  lptim->CR |= LPTIM_CR_ENABLE;
  lptim->CR |= LPTIM_CR_CNTSTRT;
}

static void prv_display_off() {
  DisplayJDIState *state = DISPLAY->state;
  HAL_LCDC_DeInit(&state->hlcdc);

  LPTIM_TypeDef *lptim = DISPLAY->vcom.lptim;

  lptim->CR &= ~LPTIM_CR_ENABLE;
  lptim->CR &= ~LPTIM_CR_CNTSTRT;

  delay_us(POWER_SEQ_DELAY_TIME_US);
  gpio_output_set(&DISPLAY->vddp, false);
  delay_us(POWER_SEQ_DELAY_TIME_US);
  gpio_output_set(&DISPLAY->vlcd, false);
}

static void prv_display_update_start(void) {
  DisplayJDIState *state = DISPLAY->state;

  // Only send the dirty region that was converted to RGB332 format
  HAL_LCDC_SetROIArea(&state->hlcdc, 0, s_update_y0, PBL_DISPLAY_WIDTH - 1, s_update_y1);
  HAL_LCDC_LayerSetData(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, s_framebuffer, 0, s_update_y0,
                        PBL_DISPLAY_WIDTH - 1, s_update_y1);
  HAL_LCDC_SendLayerData_IT(&state->hlcdc);
}

static void prv_display_update_terminate(void *data) {
  // Convert the updated region back from 332 to 222 format
  for (uint16_t y = s_update_y0; y <= s_update_y1; y++) {
    uint8_t *row = &s_framebuffer[y * PBL_DISPLAY_WIDTH];

  if (s_rotated_180) {
    // Undo HMirror before converting back
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 2; x++) {
      uint8_t tmp = row[x];
      row[x] = row[PBL_DISPLAY_WIDTH - 1 - x];
      row[PBL_DISPLAY_WIDTH - 1 - x] = tmp;
    }
  }

    // Convert this row in-place from 332 to 222 using word-level bit manipulation
    // 332 format: RR 0G GG BB (bits 7-6 R, 4-3 G, 1-0 B)
    // 222 format: XX RR GG BB (bits 7-6 unused, 5-4 R, 3-2 G, 1-0 B)
    uint32_t *row32 = (uint32_t *)row;
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 4; x++) {
      uint32_t p = row32[x];
      row32[x] = ((p >> 2) & 0x30303030) |  // R: bits 6-7 → 4-5
                 ((p >> 1) & 0x0C0C0C0C) |  // G: bits 3-4 → 2-3
                 (p & 0x03030303);          // B: bits 0-1 stay
    }
  }

  s_updating = false;
  s_uccb();
  stop_mode_enable(InhibitorDisplay);
}

void display_jdi_irq_handler(DisplayJDIDevice *disp) {
  DisplayJDIState *state = DISPLAY->state;
  HAL_LCDC_IRQHandler(&state->hlcdc);
}

void HAL_LCDC_SendLayerDataCpltCbk(LCDC_HandleTypeDef *lcdc) {
  portBASE_TYPE woken = pdFALSE;

  if (s_updating) {
    PebbleEvent e = {
        .type = PEBBLE_CALLBACK_EVENT,
        .callback =
            {
                .callback = prv_display_update_terminate,
            },
    };

    woken = event_put_isr(&e) ? pdTRUE : pdFALSE;
  } else {
    xSemaphoreGiveFromISR(s_sem, &woken);
  }

  portEND_SWITCHING_ISR(woken);
}

void display_init(void) {
  if (s_initialized) {
    return;
  }

  DisplayJDIState *state = DISPLAY->state;

  gpio_output_init(&DISPLAY->vddp, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_init(&DISPLAY->vlcd, GPIO_OType_PP, GPIO_Speed_2MHz);

  prv_power_cycle();

  HAL_PIN_Set(DISPLAY->pinmux.xrst.pad, DISPLAY->pinmux.xrst.func, DISPLAY->pinmux.xrst.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.vst.pad, DISPLAY->pinmux.vst.func, DISPLAY->pinmux.vst.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.vck.pad, DISPLAY->pinmux.vck.func, DISPLAY->pinmux.vck.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.enb.pad, DISPLAY->pinmux.enb.func, DISPLAY->pinmux.enb.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.hst.pad, DISPLAY->pinmux.hst.func, DISPLAY->pinmux.hst.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.hck.pad, DISPLAY->pinmux.hck.func, DISPLAY->pinmux.hck.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.r1.pad, DISPLAY->pinmux.r1.func, DISPLAY->pinmux.r1.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.r2.pad, DISPLAY->pinmux.r2.func, DISPLAY->pinmux.r2.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.g1.pad, DISPLAY->pinmux.g1.func, DISPLAY->pinmux.g1.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.g2.pad, DISPLAY->pinmux.g2.func, DISPLAY->pinmux.g2.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.b1.pad, DISPLAY->pinmux.b1.func, DISPLAY->pinmux.b1.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.b2.pad, DISPLAY->pinmux.b2.func, DISPLAY->pinmux.b2.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.vcom_frp.pad, DISPLAY->pinmux.vcom_frp.func, DISPLAY->pinmux.vcom_frp.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.xfrp.pad, DISPLAY->pinmux.xfrp.func, DISPLAY->pinmux.xfrp.flags, 1);

  HAL_LCDC_Init(&state->hlcdc);
  HAL_LCDC_LayerReset(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT);
  HAL_LCDC_LayerSetCmpr(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, 0);
  HAL_LCDC_LayerSetFormat(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB332);
  HAL_LCDC_LayerVMirror(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, s_rotated_180);

  HAL_NVIC_SetPriority(DISPLAY->irqn, DISPLAY->irq_priority, 0);
  HAL_NVIC_EnableIRQ(DISPLAY->irqn);

  s_sem = xSemaphoreCreateBinary();

  prv_display_on();

  s_initialized = true;
}

void display_set_enabled(bool enabled) {
  if (enabled) {
    prv_display_on();
  } else {
    prv_display_off();
  }
}

bool display_update_in_progress(void) {
  return s_updating;
}

void display_set_rotated(bool rotated) {
  DisplayJDIState *state = DISPLAY->state;

#if DISPLAY_ORIENTATION_ROTATED_180
  s_rotated_180 = !rotated;
#else
  s_rotated_180 = rotated;
#endif
  HAL_LCDC_LayerVMirror(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, s_rotated_180);

}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  DisplayRow row;
  bool first_row = true;

  PBL_ASSERTN(!s_updating);

  // Convert rows in-place from 222 to 332 format
  // We use the compositor's framebuffer directly to save RAM
  while (nrcb(&row)) {
    if (first_row) {
      // Capture pointer to compositor's framebuffer from first row
      s_framebuffer = row.data;
      s_update_y0 = row.address;
      first_row = false;
    }
    s_update_y1 = row.address;

    // Convert this row in-place from 222 to 332 using word-level bit manipulation
    // 222 format: XX RR GG BB (bits 7-6 unused, 5-4 R, 3-2 G, 1-0 B)
    // 332 format: RR 0G GG BB (bits 7-6 R, 4-3 G, 1-0 B)
    uint32_t *row32 = (uint32_t *)row.data;
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 4; x++) {
      uint32_t p = row32[x];
      row32[x] = ((p & 0x30303030) << 2) |  // R: bits 4-5 → 6-7
                 ((p & 0x0C0C0C0C) << 1) |  // G: bits 2-3 → 3-4
                 (p & 0x03030303);          // B: bits 0-1 stay
    }

    if (s_rotated_180) {
      // HMirror in software (VMirror is done by hardware)
      uint8_t *row_data = row.data;
      for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 2; x++) {
        uint8_t tmp = row_data[x];
        row_data[x] = row_data[PBL_DISPLAY_WIDTH - 1 - x];
        row_data[PBL_DISPLAY_WIDTH - 1 - x] = tmp;
      }
    }
  }

  if (first_row) {
    // No rows to update
    uccb();
    return;
  }

  // Adjust framebuffer pointer to start of buffer (row 0)
  s_framebuffer = s_framebuffer - (s_update_y0 * PBL_DISPLAY_WIDTH);

  s_uccb = uccb;
  s_updating = true;

  stop_mode_disable(InhibitorDisplay);
  prv_display_update_start();
}

void display_update_boot_frame(uint8_t *framebuffer) {
  if (s_rotated_180) {
    // HMirror in software (VMirror is done by hardware)
    for (uint16_t y = 0; y < PBL_DISPLAY_HEIGHT; y++) {
      uint8_t *row = &framebuffer[y * PBL_DISPLAY_WIDTH];
      for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 2; x++) {
        uint8_t tmp = row[x];
        row[x] = row[PBL_DISPLAY_WIDTH - 1 - x];
        row[PBL_DISPLAY_WIDTH - 1 - x] = tmp;
      }
    }
  }

  s_framebuffer = framebuffer;
  s_update_y0 = 0;
  s_update_y1 = PBL_DISPLAY_HEIGHT - 1;

  stop_mode_disable(InhibitorDisplay);
  prv_display_update_start();
  xSemaphoreTake(s_sem, portMAX_DELAY);
  stop_mode_enable(InhibitorDisplay);
}

void display_clear(void) {}

uint32_t display_baud_rate_change(uint32_t new_frequency_hz) { return 0U; }