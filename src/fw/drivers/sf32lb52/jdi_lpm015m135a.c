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

static uint8_t s_framebuffer[DISPLAY_FRAMEBUFFER_BYTES];
static PebbleMutex *s_update;
static SemaphoreHandle_t s_write_done;

static void prv_fb_222_to_332(const DisplayRow *row) {
  for (uint16_t count = 0U; count < PBL_DISPLAY_WIDTH; count++) {
    s_framebuffer[row->address * PBL_DISPLAY_WIDTH + count] = BYTE_222_TO_332(*(row->data + count));
  }
}

// TODO(SF32LB52): Improve/clarify display on/off code
static void prv_display_on() {
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
}

void jdi_lpm015m135a_irq_handler(DisplayJDIDevice *disp) {
  DisplayJDIState *state = DISPLAY->state;
  HAL_LCDC_IRQHandler(&state->hlcdc);
}

void HAL_LCDC_SendLayerDataCpltCbk(LCDC_HandleTypeDef *lcdc) {
  BaseType_t woken;
  xSemaphoreGiveFromISR(s_write_done, &woken);
  portYIELD_FROM_ISR(woken);
}

void display_init(void) {
  DisplayJDIState *state = DISPLAY->state;

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
                      .bank_col_head = 0,
                      .valid_columns = PBL_DISPLAY_WIDTH,
                      .bank_col_tail = 8,
                      .bank_row_head = 0,
                      .valid_rows = PBL_DISPLAY_HEIGHT,
                      .bank_row_tail = 4,
                  },
          },
  };

  HAL_LCDC_Init(&state->hlcdc);
  HAL_LCDC_LayerReset(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT);
  HAL_LCDC_LayerSetCmpr(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, 0);
  HAL_LCDC_LayerSetFormat(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB332);

  HAL_NVIC_SetPriority(DISPLAY->irqn, DISPLAY->irq_priority, 0);
  HAL_NVIC_EnableIRQ(DISPLAY->irqn);

  HAL_LCDC_Enter_LP(&state->hlcdc);

  s_update = mutex_create();
  vSemaphoreCreateBinary(s_write_done);

  display_clear();
  prv_display_on();
}

void display_clear(void) {
  DisplayJDIState *state = DISPLAY->state;

  mutex_lock(s_update);

  memset(s_framebuffer, 0xFF, DISPLAY_FRAMEBUFFER_BYTES);

  HAL_LCDC_Exit_LP(&state->hlcdc);
  HAL_LCDC_SetROIArea(&state->hlcdc, 0, 0, PBL_DISPLAY_WIDTH - 1, PBL_DISPLAY_HEIGHT - 1);
  HAL_LCDC_LayerSetData(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, s_framebuffer, 0, 0,
                        PBL_DISPLAY_WIDTH - 1, PBL_DISPLAY_HEIGHT - 1);
  HAL_LCDC_SendLayerData_IT(&state->hlcdc);
  xSemaphoreTake(s_write_done, portMAX_DELAY);
  HAL_LCDC_Enter_LP(&state->hlcdc);

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

  mutex_lock(s_update);

  stop_mode_disable(InhibitorDisplay);

  // convert all rows requiring an update to 332 format
  while (nrcb(&row)) {
    if (!y0_set) {
      y0 = row.address;
      y0_set = true;
    }

    prv_fb_222_to_332(&row);

    rows++;
  }

  if (rows > 0U) {
    HAL_LCDC_Exit_LP(&state->hlcdc);
    HAL_LCDC_SetROIArea(&state->hlcdc, 0, y0, PBL_DISPLAY_WIDTH - 1, y0 + rows - 1);
    HAL_LCDC_LayerSetData(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT,
                          &s_framebuffer[y0 * PBL_DISPLAY_WIDTH], 0, y0, PBL_DISPLAY_WIDTH - 1,
                          y0 + rows - 1);
    HAL_LCDC_SendLayerData_IT(&state->hlcdc);
    xSemaphoreTake(s_write_done, portMAX_DELAY);
    HAL_LCDC_Enter_LP(&state->hlcdc);
  }

  if (uccb) {
    uccb();
  }

  stop_mode_enable(InhibitorDisplay);

  // FIXME(SF32LB52): !HACK ALERT! Adding delay here to mitigate potential race
  psleep(20);

  mutex_unlock(s_update);
}

void display_pulse_vcom(void) {}

void display_show_splash_screen(void) {}

void display_show_panic_screen(uint32_t error_code) {}

uint32_t display_baud_rate_change(uint32_t new_frequency_hz) { return 0U; }

// Stubs for display offset
void display_set_offset(GPoint offset) {}

GPoint display_get_offset(void) { return GPointZero; }
