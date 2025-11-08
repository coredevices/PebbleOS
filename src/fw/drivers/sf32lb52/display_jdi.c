/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "display_jdi.h"

#include "board/board.h"
#include "board/display.h"
#include "drivers/display/display.h"
#include "drivers/gpio.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
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

const uint8_t s_222_to_332_lut[256] = {
0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,
0x40, 0x41, 0x42, 0x43, 0x48, 0x49, 0x4a, 0x4b, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5a, 0x5b,
0x80, 0x81, 0x82, 0x83, 0x88, 0x89, 0x8a, 0x8b, 0x90, 0x91, 0x92, 0x93, 0x98, 0x99, 0x9a, 0x9b,
0xc0, 0xc1, 0xc2, 0xc3, 0xc8, 0xc9, 0xca, 0xcb, 0xd0, 0xd1, 0xd2, 0xd3, 0xd8, 0xd9, 0xda, 0xdb,
0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,
0x40, 0x41, 0x42, 0x43, 0x48, 0x49, 0x4a, 0x4b, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5a, 0x5b,
0x80, 0x81, 0x82, 0x83, 0x88, 0x89, 0x8a, 0x8b, 0x90, 0x91, 0x92, 0x93, 0x98, 0x99, 0x9a, 0x9b,
0xc0, 0xc1, 0xc2, 0xc3, 0xc8, 0xc9, 0xca, 0xcb, 0xd0, 0xd1, 0xd2, 0xd3, 0xd8, 0xd9, 0xda, 0xdb,
0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,
0x40, 0x41, 0x42, 0x43, 0x48, 0x49, 0x4a, 0x4b, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5a, 0x5b,
0x80, 0x81, 0x82, 0x83, 0x88, 0x89, 0x8a, 0x8b, 0x90, 0x91, 0x92, 0x93, 0x98, 0x99, 0x9a, 0x9b,
0xc0, 0xc1, 0xc2, 0xc3, 0xc8, 0xc9, 0xca, 0xcb, 0xd0, 0xd1, 0xd2, 0xd3, 0xd8, 0xd9, 0xda, 0xdb,
0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b,
0x40, 0x41, 0x42, 0x43, 0x48, 0x49, 0x4a, 0x4b, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5a, 0x5b,
0x80, 0x81, 0x82, 0x83, 0x88, 0x89, 0x8a, 0x8b, 0x90, 0x91, 0x92, 0x93, 0x98, 0x99, 0x9a, 0x9b,
0xc0, 0xc1, 0xc2, 0xc3, 0xc8, 0xc9, 0xca, 0xcb, 0xd0, 0xd1, 0xd2, 0xd3, 0xd8, 0xd9, 0xda, 0xdb
};

const uint8_t s_332_to_222_lut[256] = {
0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,
0x08, 0x09, 0x0a, 0x0b, 0x08, 0x09, 0x0a, 0x0b, 0x08, 0x09, 0x0a, 0x0b, 0x08, 0x09, 0x0a, 0x0b,
0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13,
0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b,
0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13,
0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b,
0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13, 0x10, 0x11, 0x12, 0x13,
0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b, 0x18, 0x19, 0x1a, 0x1b,
0x20, 0x21, 0x22, 0x23, 0x20, 0x21, 0x22, 0x23, 0x20, 0x21, 0x22, 0x23, 0x20, 0x21, 0x22, 0x23,
0x28, 0x29, 0x2a, 0x2b, 0x28, 0x29, 0x2a, 0x2b, 0x28, 0x29, 0x2a, 0x2b, 0x28, 0x29, 0x2a, 0x2b,
0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33,
0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b,
0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33,
0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b,
0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33,
0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b, 0x38, 0x39, 0x3a, 0x3b
};

#define BYTE_222_TO_332(data) (s_222_to_332_lut[(data)])
#define BYTE_332_TO_222(data) (s_332_to_222_lut[(data)])

#define POWER_SEQ_DELAY_TIME  (11)
#define POWER_RESET_CYCLE_DELAY_TIME (500)

// Pointer to the compositor's framebuffer - we convert in-place to save 44KB RAM
static uint8_t *s_framebuffer;
static uint16_t s_update_y0;
static uint16_t s_update_y1;
static bool s_initialized;
static bool s_updating;
static UpdateCompleteCallback s_uccb;
static SemaphoreHandle_t s_sem;

static void prv_power_cycle(void){
  OutputConfig cfg = {
    .gpio = hwp_gpio1,
    .active_high = false,
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

  cfg.gpio_pin = DISPLAY->vlcd.gpio_pin;
  gpio_output_init(&cfg, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&cfg, false);

  psleep(POWER_RESET_CYCLE_DELAY_TIME);
}

// TODO(SF32LB52): Improve/clarify display on/off code
static void prv_display_on() {
  // FIXME(OBELIX, GETAFIX): GPIO logic level should be specified at board level
#if PLATFORM_OBELIX
  gpio_output_set(&DISPLAY->vlcd, false);
#elif PLATFORM_GETAFIX
  gpio_output_set(&DISPLAY->vlcd, true);
#endif
  psleep(POWER_SEQ_DELAY_TIME);
  gpio_output_set(&DISPLAY->vddp, true);
  psleep(POWER_SEQ_DELAY_TIME);

  LPTIM_TypeDef *lptim = DISPLAY->vcom.lptim;

  lptim->CFGR |= LPTIM_INTCLOCKSOURCE_LPCLOCK;
  lptim->ARR = RC10K_FREQ / DISPLAY->vcom.freq_hz;
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
  DisplayJDIState *state = DISPLAY->state;
  HAL_LCDC_DeInit(&state->hlcdc);

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

  psleep(POWER_SEQ_DELAY_TIME);
  gpio_output_set(&DISPLAY->vddp, false);
  psleep(POWER_SEQ_DELAY_TIME);
  // FIXME(OBELIX, GETAFIX): GPIO logic level should be specified at board level
#if PLATFORM_OBELIX
  gpio_output_set(&DISPLAY->vlcd, true);
#elif PLATFORM_GETAFIX
  gpio_output_set(&DISPLAY->vlcd, false);
#endif
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

#if DISPLAY_ORIENTATION_ROTATED_180
    // Undo HMirror before converting back
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 2; x++) {
      uint8_t tmp = row[x];
      row[x] = row[PBL_DISPLAY_WIDTH - 1 - x];
      row[PBL_DISPLAY_WIDTH - 1 - x] = tmp;
    }
#endif

    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH; x++) {
      row[x] = BYTE_332_TO_222(row[x]);
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

  prv_power_cycle();

  gpio_output_init(&DISPLAY->vddp, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_init(&DISPLAY->vlcd, GPIO_OType_PP, GPIO_Speed_2MHz);

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

  HAL_LCDC_Init(&state->hlcdc);
  HAL_LCDC_LayerReset(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT);
  HAL_LCDC_LayerSetCmpr(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, 0);
  HAL_LCDC_LayerSetFormat(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB332);
#if DISPLAY_ORIENTATION_ROTATED_180
  // sf32lb52 hw only supports VMirror, do HMirror in software
  HAL_LCDC_LayerVMirror(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, true);
#endif

  HAL_NVIC_SetPriority(DISPLAY->irqn, DISPLAY->irq_priority, 0);
  HAL_NVIC_EnableIRQ(DISPLAY->irqn);

  s_sem = xSemaphoreCreateBinary();

  prv_display_on();

  s_initialized = true;
}

void display_clear(void) {
  DisplayJDIState *state = DISPLAY->state;

  // Allocate temporary framebuffer for clear operation
  // This is only called during boot when heap has plenty of space
  uint8_t *temp_fb = kernel_malloc(DISPLAY_FRAMEBUFFER_BYTES);
  if (!temp_fb) {
    return;
  }
  
  memset(temp_fb, 0xFF, DISPLAY_FRAMEBUFFER_BYTES);
  s_framebuffer = temp_fb;
  s_update_y0 = 0;
  s_update_y1 = PBL_DISPLAY_HEIGHT - 1;

  stop_mode_disable(InhibitorDisplay);
  prv_display_update_start();
  xSemaphoreTake(s_sem, portMAX_DELAY);
  stop_mode_enable(InhibitorDisplay);
  
  kernel_free(temp_fb);
  s_framebuffer = NULL;
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

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  DisplayJDIState *state = DISPLAY->state;
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

    // Convert this row in-place from 222 to 332
    uint8_t *row_data = row.data;
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH; x++) {
      row_data[x] = BYTE_222_TO_332(row_data[x]);
    }

#if DISPLAY_ORIENTATION_ROTATED_180
    // HMirror in software (VMirror is done by hardware)
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 2; x++) {
      uint8_t tmp = row_data[x];
      row_data[x] = row_data[PBL_DISPLAY_WIDTH - 1 - x];
      row_data[PBL_DISPLAY_WIDTH - 1 - x] = tmp;
    }
#endif
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

  // Allocate temporary framebuffer for splash screen
  // This is only called during boot when heap has plenty of space
  uint8_t *temp_fb = kernel_malloc(DISPLAY_FRAMEBUFFER_BYTES);
  if (!temp_fb) {
    return;
  }

  memset(temp_fb, 0xFF, DISPLAY_FRAMEBUFFER_BYTES);

  x0 = (PBL_DISPLAY_WIDTH - splash->width) / 2;
  y0 = (PBL_DISPLAY_HEIGHT - splash->height) / 2;
  for (uint16_t y = 0U; y < splash->height; y++) {
    for (uint16_t x = 0U; x < splash->width; x++) {
      if (splash->data[y * (splash->width / 8) + x / 8] & (0x1U << (x & 7))) {
        temp_fb[(y + y0) * PBL_DISPLAY_WIDTH + (x + x0)] = 0x00;
      }
    }
  }

#if DISPLAY_ORIENTATION_ROTATED_180
  // HMirror in software (VMirror is done by hardware)
  for (uint16_t y = 0; y < PBL_DISPLAY_HEIGHT; y++) {
    uint8_t *row = &temp_fb[y * PBL_DISPLAY_WIDTH];
    for (uint16_t x = 0; x < PBL_DISPLAY_WIDTH / 2; x++) {
      uint8_t tmp = row[x];
      row[x] = row[PBL_DISPLAY_WIDTH - 1 - x];
      row[PBL_DISPLAY_WIDTH - 1 - x] = tmp;
    }
  }
#endif

  s_framebuffer = temp_fb;
  s_update_y0 = 0;
  s_update_y1 = PBL_DISPLAY_HEIGHT - 1;

  stop_mode_disable(InhibitorDisplay);
  prv_display_update_start();
  xSemaphoreTake(s_sem, portMAX_DELAY);
  stop_mode_enable(InhibitorDisplay);
  
  kernel_free(temp_fb);
  s_framebuffer = NULL;
}

void display_pulse_vcom(void) {}

void display_show_panic_screen(uint32_t error_code) {}

uint32_t display_baud_rate_change(uint32_t new_frequency_hz) { return 0U; }

// Stubs for display offset
void display_set_offset(GPoint offset) {}

GPoint display_get_offset(void) { return GPointZero; }
