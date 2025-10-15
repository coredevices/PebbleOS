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

#include "jdi_lpm015m135a.h"

#include "board/board.h"
#include "board/display.h"
#include "drivers/display/display.h"
#include "drivers/gpio.h"
#include "kernel/util/sleep.h"
#include "kernel/util/stop.h"
#include "os/mutex.h"
#include "system/logging.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "bf0_hal.h"
#include "bf0_hal_lcdc.h"
#include "bf0_hal_lptim.h"

#define BYTE_222_TO_332(data) ((((data) & 0x30) << 2) | (((data) & 0x0c) << 1) | ((data) & 0x03))
#define POWER_SEQ_DELAY_TIME  (11)

static bool s_initialized;
static uint8_t s_framebuffer[2][DISPLAY_FRAMEBUFFER_BYTES];
static bool s_framebuffer_dirty[2];
static uint8_t s_framebuffer_index;
static PebbleMutex *s_update;
static SemaphoreHandle_t s_write_done;

// TODO(SF32LB52): Improve/clarify display on/off code
static void prv_display_on() {
  // FIXME(SF32LB52): make this mandatory once old big boards are gone
  if (DISPLAY->vlcd.gpio != NULL && DISPLAY->vddp.gpio != NULL) {
    gpio_output_set(&DISPLAY->vlcd, true);
    psleep(POWER_SEQ_DELAY_TIME);
    gpio_output_set(&DISPLAY->vddp, true);
    psleep(POWER_SEQ_DELAY_TIME);
  }

  LPTIM_TypeDef *lptim = DISPLAY->vcom.lptim;

  lptim->CFGR |= LPTIM_INTLOCKSOURCE_APBCLOCK;
  lptim->ARR = 3750000 / DISPLAY->vcom.freq_hz;
  lptim->CMP = lptim->ARR / 2;
  lptim->CR |= LPTIM_CR_ENABLE;
  lptim->CR |= LPTIM_CR_CNTSTRT;

  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL0_Msk, 3 << HPSYS_AON_CR1_PINOUT_SEL0_Pos);
  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL1_Msk, 3 << HPSYS_AON_CR1_PINOUT_SEL1_Pos);

  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_SEL_Msk, 3 << RTC_PBR0R_SEL_Pos);
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_SEL_Msk, 2 << RTC_PBR1R_SEL_Pos);

  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_OE_Msk, 1 << RTC_PBR0R_OE_Pos);
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_OE_Msk, 1 << RTC_PBR1R_OE_Pos);
}

static void prv_display_off() {
  LPTIM_TypeDef *lptim = DISPLAY->vcom.lptim;

  lptim->CR &= ~LPTIM_CR_ENABLE;
  lptim->CR &= ~LPTIM_CR_CNTSTRT;

  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL0_Msk, 0 << HPSYS_AON_CR1_PINOUT_SEL0_Pos);
  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL1_Msk, 0 << HPSYS_AON_CR1_PINOUT_SEL1_Pos);

  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_SEL_Msk | RTC_PBR0R_OE_Msk, 0);
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_SEL_Msk | RTC_PBR1R_OE_Msk, 0);

  // IE=0, PE=0, OE=0
  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_IE_Msk | RTC_PBR0R_PE_Msk | RTC_PBR0R_OE_Msk, 0);
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_IE_Msk | RTC_PBR1R_PE_Msk | RTC_PBR1R_OE_Msk, 0);

  // FIXME(SF32LB52): make this mandatory once old big boards are gone
  if (DISPLAY->vlcd.gpio != NULL && DISPLAY->vddp.gpio != NULL) {
    psleep(POWER_SEQ_DELAY_TIME);
    gpio_output_set(&DISPLAY->vddp, true);
    psleep(POWER_SEQ_DELAY_TIME);
    gpio_output_set(&DISPLAY->vlcd, true);
  }
}

static void prv_display_update(uint8_t index) {
  DisplayJDIState *state = DISPLAY->state;

  HAL_LCDC_SetROIArea(&state->hlcdc, 0, 0, PBL_DISPLAY_WIDTH - 1, PBL_DISPLAY_HEIGHT - 1);
  HAL_LCDC_LayerSetData(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, s_framebuffer[index], 0, 0,
                        PBL_DISPLAY_WIDTH - 1, PBL_DISPLAY_HEIGHT - 1);
  HAL_LCDC_SendLayerData_IT(&state->hlcdc);
}

void jdi_lpm015m135a_irq_handler(DisplayJDIDevice *disp) {
  DisplayJDIState *state = DISPLAY->state;
  HAL_LCDC_IRQHandler(&state->hlcdc);
}

void HAL_LCDC_SendLayerDataCpltCbk(LCDC_HandleTypeDef *lcdc) {
  uint8_t index = s_framebuffer_index ^ 1;

  s_framebuffer_dirty[s_framebuffer_index] = false;

  if (s_framebuffer_dirty[index]) {
    prv_display_update(index);
    s_framebuffer_index = index;
  } else {
    stop_mode_enable(InhibitorDisplay);
  }
}

void display_init(void) {
  if (s_initialized) {
    return;
  }

  DisplayJDIState *state = DISPLAY->state;

  // FIXME(SF32LB52): make this mandatory once old big boards are gone
  if (DISPLAY->vlcd.gpio != NULL && DISPLAY->vddp.gpio != NULL) {
    gpio_output_init(&DISPLAY->vddp, GPIO_OType_PP, GPIO_Speed_2MHz);
    gpio_output_init(&DISPLAY->vlcd, GPIO_OType_PP, GPIO_Speed_2MHz);
  }

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
  HAL_PIN_Set(DISPLAY->pinmux.vcom.pad, DISPLAY->pinmux.vcom.func, DISPLAY->pinmux.vcom.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.va.pad, DISPLAY->pinmux.va.func, DISPLAY->pinmux.va.flags, 1);
  HAL_PIN_Set(DISPLAY->pinmux.vb.pad, DISPLAY->pinmux.vb.func, DISPLAY->pinmux.vb.flags, 1);

  state->hlcdc.Init = (LCDC_InitTypeDef){
      .lcd_itf = LCDC_INTF_JDI_PARALLEL,
      .color_mode = LCDC_PIXEL_FORMAT_RGB332,
      .freq = 746268,  // HCK frequency
      .cfg =
          {
              .jdi =
                  (JDI_LCD_CFG){
                      .bank_col_head = 2,
                      .valid_columns = PBL_DISPLAY_WIDTH,
                      .bank_col_tail = 6,
                      .bank_row_head = 0,
                      .valid_rows = PBL_DISPLAY_HEIGHT,
                      .bank_row_tail = 6,
                      .enb_start_col = 3,
                      .enb_end_col = 99,
                  },
          },
  };


  HAL_LCDC_Init(&state->hlcdc);
  HAL_LCDC_LayerReset(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT);
  HAL_LCDC_LayerSetCmpr(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, 0);
  HAL_LCDC_LayerSetFormat(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB332);

  HAL_NVIC_SetPriority(DISPLAY->irqn, DISPLAY->irq_priority, 0);
  HAL_NVIC_EnableIRQ(DISPLAY->irqn);

  s_update = mutex_create();
  vSemaphoreCreateBinary(s_write_done);

  prv_display_on();

  s_initialized = true;
}

void display_clear(void) {
  DisplayJDIState *state = DISPLAY->state;
  uint8_t index;

  mutex_lock(s_update);

  portENTER_CRITICAL();
  if (!s_framebuffer_dirty[s_framebuffer_index]) {
    index = s_framebuffer_index;
  } else if (!s_framebuffer_dirty[s_framebuffer_index ^ 1]) {
    index = s_framebuffer_index ^ 1;
  } else {
    index = s_framebuffer_index ^ 1;
    s_framebuffer_dirty[index] = false;
  }
  portEXIT_CRITICAL();

  memset(s_framebuffer[index], 0xFF, DISPLAY_FRAMEBUFFER_BYTES);

  portENTER_CRITICAL();
  s_framebuffer_dirty[index] = true;
  portEXIT_CRITICAL();

  if (!(state->hlcdc.Instance->STATUS & LCD_IF_STATUS_JDI_PAR_RUN)) {
    stop_mode_disable(InhibitorDisplay);
    prv_display_update(index);
  }

  mutex_unlock(s_update);
}

void display_set_enabled(bool enabled) {
  if (enabled) {
    prv_display_on();
  } else {
    prv_display_off();
  }
}

bool display_update_in_progress(void) {
  bool in_progress = !mutex_lock_with_timeout(s_update, 0);
  if (!in_progress) {
    mutex_unlock(s_update);
  }

  return in_progress;
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  DisplayJDIState *state = DISPLAY->state;
  DisplayRow row;
  uint16_t rows = 0U;
  uint16_t y0 = 0U;
  bool y0_set = false;
  uint8_t index;

  mutex_lock(s_update);

  portENTER_CRITICAL();
  if (!s_framebuffer_dirty[s_framebuffer_index]) {
    index = s_framebuffer_index;
  } else if (!s_framebuffer_dirty[s_framebuffer_index ^ 1]) {
    index = s_framebuffer_index ^ 1;
  } else {
    index = s_framebuffer_index ^ 1;
    s_framebuffer_dirty[index] = false;
  }
  portEXIT_CRITICAL();

  // convert all rows requiring an update to 332 format
  while (nrcb(&row)) {
    if (!y0_set) {
      y0 = row.address;
      y0_set = true;
    }

    for (uint16_t count = 0U; count < PBL_DISPLAY_WIDTH; count++) {
      s_framebuffer[index][row.address * PBL_DISPLAY_WIDTH + count] = BYTE_222_TO_332(*(row.data + count));
    }

    rows++;
  }

  portENTER_CRITICAL();
  s_framebuffer_dirty[index] = true;
  portEXIT_CRITICAL();

  if (!(state->hlcdc.Instance->STATUS & LCD_IF_STATUS_JDI_PAR_RUN)) {
    stop_mode_disable(InhibitorDisplay);
    prv_display_update(index);
  }

  if (uccb) {
    uccb();
  }

  mutex_unlock(s_update);
}

void display_show_splash_screen(void) {
  const DisplayJDISplash *splash = &DISPLAY->splash;
  uint16_t x0, y0;

  if (splash->data == NULL) {
    return;
  }

  if (splash->width > PBL_DISPLAY_WIDTH || splash->height > PBL_DISPLAY_HEIGHT) {
    return;
  }

  display_init();

  memset(s_framebuffer[0], 0xFF, DISPLAY_FRAMEBUFFER_BYTES);

  x0 = (PBL_DISPLAY_WIDTH - splash->width) / 2;
  y0 = (PBL_DISPLAY_HEIGHT - splash->height) / 2;
  for (uint16_t y = 0U; y < splash->height; y++) {
    for (uint16_t x = 0U; x < splash->width; x++) {
      if (splash->data[y * (splash->width / 8) + x / 8] & (0x1U << (x & 7))) {
        s_framebuffer[0][(y + y0) * PBL_DISPLAY_WIDTH + (x + x0)] = 0x00;
      }
    }
  }

  s_framebuffer_dirty[0] = true;

  stop_mode_disable(InhibitorDisplay);
  prv_display_update(0);
}

void display_pulse_vcom(void) {}

void display_show_panic_screen(uint32_t error_code) {}

uint32_t display_baud_rate_change(uint32_t new_frequency_hz) { return 0U; }

// Stubs for display offset
void display_set_offset(GPoint offset) {}

GPoint display_get_offset(void) { return GPointZero; }
