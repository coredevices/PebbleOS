#include "FreeRTOS.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gtypes.h"
#include "bf0_hal.h"
#include "bf0_hal_lptim.h"
#include "board/board.h"
#include "board/display.h"
#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "kernel/util/sleep.h"
#include "kernel/util/stop.h"
#include "os/mutex.h"
#include "os/tick.h"
#include "queue.h"
#include "semphr.h"
#include "services/common/analytics/analytics.h"
#include "system/logging.h"
#include "system/passert.h"
#include "task.h"
#include "util/net.h"
#include "util/reverse.h"

#define JDI_FRAMEBUF_PEBBLE

#define FB_WIDTH PBL_DISPLAY_WIDTH
#define FB_HEIGHT PBL_DISPLAY_HEIGHT

#define FB_COLOR_FORMAT LCDC_PIXEL_FORMAT_RGB332        /*framebuff format*/
#define FB_PIXEL_BYTES 1
#define FB_TOTAL_BYTES (FB_WIDTH * FB_HEIGHT * FB_PIXEL_BYTES)

static uint8_t s_framebuffer[FB_TOTAL_BYTES];
static GPoint s_disp_offset;
static bool s_initialized;
static SemaphoreHandle_t s_dma_update_in_progress_semaphore;
static SemaphoreHandle_t s_display_write;

void lcd_irq_handler(LCDDevice *lcd) {
  portDISABLE_INTERRUPTS();
  HAL_LCDC_IRQHandler(&lcd->lcdc);
  portENABLE_INTERRUPTS();
}

void HAL_LCDC_SendLayerDataCpltCbk(LCDC_HandleTypeDef *lcdc) {
  static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(s_display_write, &xHigherPriorityTaskWoken);
}

void display_on() {
  hwp_lptim2->CFGR |= LPTIM_INTLOCKSOURCE_APBCLOCK;
  hwp_lptim2->ARR = 3750000 / LCD_JDI_LPM015M135A->lcdc.Init.freq;
  hwp_lptim2->CMP = hwp_lptim2->ARR / 2;
  hwp_lptim2->CR |= LPTIM_CR_ENABLE;
  hwp_lptim2->CR |= LPTIM_CR_CNTSTRT;

  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL0_Msk, 3 << HPSYS_AON_CR1_PINOUT_SEL0_Pos);
  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL1_Msk, 3 << HPSYS_AON_CR1_PINOUT_SEL1_Pos);

  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_SEL_Msk, 3 << RTC_PBR0R_SEL_Pos);
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_SEL_Msk, 2 << RTC_PBR1R_SEL_Pos);

  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_OE_Msk, 1 << RTC_PBR0R_OE_Pos);
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_OE_Msk, 1 << RTC_PBR1R_OE_Pos);
}
void display_off() {
  hwp_lptim2->CR &= ~LPTIM_CR_ENABLE;
  hwp_lptim2->CR &= ~LPTIM_CR_CNTSTRT;
  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL0_Msk, 0 << HPSYS_AON_CR1_PINOUT_SEL0_Pos);
  MODIFY_REG(hwp_hpsys_aon->CR1, HPSYS_AON_CR1_PINOUT_SEL1_Msk, 0 << HPSYS_AON_CR1_PINOUT_SEL1_Pos);

  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_SEL_Msk | RTC_PBR0R_OE_Msk, 0);
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_SEL_Msk | RTC_PBR1R_OE_Msk, 0);

  MODIFY_REG(hwp_rtc->PBR0R, RTC_PBR0R_IE_Msk | RTC_PBR0R_PE_Msk | RTC_PBR0R_OE_Msk,
             0);  // IE=0, PE=0, OE=0
  MODIFY_REG(hwp_rtc->PBR1R, RTC_PBR1R_IE_Msk | RTC_PBR1R_PE_Msk | RTC_PBR1R_OE_Msk, 0);
}

void display_set_region(LCDC_HandleTypeDef *hlcdc, uint16_t Xpos0, uint16_t Ypos0, uint16_t Xpos1,
                        uint16_t Ypos1) {
  HAL_LCDC_SetROIArea(hlcdc, 0, Ypos0, FB_WIDTH - 1, Ypos1);  // Not support partical columns
}

void display_write_multiple_pixles(LCDC_HandleTypeDef *hlcdc, const uint8_t *RGBCode,
                                   uint16_t Xpos0, uint16_t Ypos0, uint16_t Xpos1, uint16_t Ypos1) {
  uint32_t size;
  HAL_LCDC_LayerSetData(hlcdc, HAL_LCDC_LAYER_DEFAULT, (uint8_t *)RGBCode, Xpos0, Ypos0, Xpos1,
                        Ypos1);
  HAL_LCDC_SendLayerData_IT(hlcdc);
}

void display_pins_set_lcd(LCDDevice *lcd) {
  if (lcd->pin.xrst.pad) {
    HAL_PIN_Set(lcd->pin.xrst.pad, lcd->pin.xrst.func, lcd->pin.xrst.flags, 1);
  }

  if (lcd->pin.vst.pad) {
    HAL_PIN_Set(lcd->pin.vst.pad, lcd->pin.vst.func, lcd->pin.vst.flags, 1);
  }

  if (lcd->pin.vck.pad) {
    HAL_PIN_Set(lcd->pin.vck.pad, lcd->pin.vck.func, lcd->pin.vck.flags, 1);
  }

  if (lcd->pin.enb.pad) {
    HAL_PIN_Set(lcd->pin.enb.pad, lcd->pin.enb.func, lcd->pin.enb.flags, 1);
  }

  if (lcd->pin.hst.pad) {
    HAL_PIN_Set(lcd->pin.hst.pad, lcd->pin.hst.func, lcd->pin.hst.flags, 1);
  }

  if (lcd->pin.hck.pad) {
    HAL_PIN_Set(lcd->pin.hck.pad, lcd->pin.hck.func, lcd->pin.hck.flags, 1);
  }

  if (lcd->pin.r1.pad) {
    HAL_PIN_Set(lcd->pin.r1.pad, lcd->pin.r1.func, lcd->pin.r1.flags, 1);
  }

  if (lcd->pin.r2.pad) {
    HAL_PIN_Set(lcd->pin.r2.pad, lcd->pin.r2.func, lcd->pin.r2.flags, 1);
  }

  if (lcd->pin.g1.pad) {
    HAL_PIN_Set(lcd->pin.g1.pad, lcd->pin.g1.func, lcd->pin.g1.flags, 1);
  }

  if (lcd->pin.g2.pad) {
    HAL_PIN_Set(lcd->pin.g2.pad, lcd->pin.g2.func, lcd->pin.g2.flags, 1);
  }

  if (lcd->pin.b1.pad) {
    HAL_PIN_Set(lcd->pin.b1.pad, lcd->pin.b1.func, lcd->pin.b1.flags, 1);
  }

  if (lcd->pin.b2.pad) {
    HAL_PIN_Set(lcd->pin.b2.pad, lcd->pin.b2.func, lcd->pin.b2.flags, 1);
  }

  if (lcd->pin.vcom.pad) {
    HAL_PIN_Set(lcd->pin.vcom.pad, lcd->pin.vcom.func, lcd->pin.vcom.flags, 1);
  }

  if (lcd->pin.va.pad) {
    HAL_PIN_Set(lcd->pin.va.pad, lcd->pin.va.func, lcd->pin.va.flags, 1);
  }

  if (lcd->pin.vb.pad) {
    HAL_PIN_Set(lcd->pin.vb.pad, lcd->pin.vb.func, lcd->pin.vb.flags, 1);
  }
}

void display_init(void) {
  display_pins_set_lcd(LCD_JDI_LPM015M135A);
  HAL_LCDC_Init(&LCD_JDI_LPM015M135A->lcdc);

  HAL_LCDC_LayerReset(&LCD_JDI_LPM015M135A->lcdc,
                      HAL_LCDC_LAYER_DEFAULT); /*Set default layer configuration*/
  HAL_LCDC_LayerSetCmpr(&LCD_JDI_LPM015M135A->lcdc, HAL_LCDC_LAYER_DEFAULT,
                        0); /*Disable layer compress*/
  HAL_LCDC_LayerSetFormat(&LCD_JDI_LPM015M135A->lcdc, HAL_LCDC_LAYER_DEFAULT, FB_COLOR_FORMAT);

  HAL_NVIC_SetPriority(LCDC1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(LCDC1_IRQn);

  HAL_LCDC_Enter_LP(&LCD_JDI_LPM015M135A->lcdc);
  vSemaphoreCreateBinary(s_dma_update_in_progress_semaphore);
  vSemaphoreCreateBinary(s_display_write);
  s_initialized = true;
  display_clear();
  display_on();
}

uint32_t display_baud_rate_change(uint32_t new_frequency_hz) {
  HAL_LCDC_SetFreq(&LCD_JDI_LPM015M135A->lcdc, new_frequency_hz);
  return new_frequency_hz;
}

void display_clear(void) {
  xSemaphoreTake(s_dma_update_in_progress_semaphore, portMAX_DELAY);

  memset(s_framebuffer, 0xFF, FB_TOTAL_BYTES);
  HAL_LCDC_Exit_LP(&LCD_JDI_LPM015M135A->lcdc);
  display_set_region(&LCD_JDI_LPM015M135A->lcdc, 0, 0, FB_WIDTH - 1, FB_HEIGHT - 1);
  display_write_multiple_pixles(&LCD_JDI_LPM015M135A->lcdc, (uint8_t *)s_framebuffer, 0, 0,
                                FB_WIDTH - 1, FB_HEIGHT - 1);
  PBL_ASSERTN(xSemaphoreTake(s_display_write, portMAX_DELAY) == pdPASS);
  HAL_LCDC_Enter_LP(&LCD_JDI_LPM015M135A->lcdc);
  xSemaphoreGive(s_dma_update_in_progress_semaphore);
}

void display_set_enabled(bool enabled) {
  if (enabled) {
    display_on();
  } else {
    display_off();
  }
}

bool display_update_in_progress(void) {
  if (xSemaphoreTake(s_dma_update_in_progress_semaphore, 0) == pdPASS) {
    xSemaphoreGive(s_dma_update_in_progress_semaphore);
    return false;
  }

  return true;
}

#define BYTE_222_TO_332(data) (((data & 0x30) << 2) | ((data & 0x0c) << 1) | (data & 0x03))
void display_framebuf_222_to_332(uint8_t *data) {
  uint16_t count = 0;
  for (count = 0; count < FB_WIDTH * FB_HEIGHT; count++) {
    s_framebuffer[count] = BYTE_222_TO_332(*(data + count));
  }
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  xSemaphoreTake(s_dma_update_in_progress_semaphore, portMAX_DELAY);

  FrameBuffer *fb = compositor_get_framebuffer();
  display_framebuf_222_to_332(fb->buffer);

  HAL_LCDC_Exit_LP(&LCD_JDI_LPM015M135A->lcdc);
  display_set_region(&LCD_JDI_LPM015M135A->lcdc, 0, 0, FB_WIDTH - 1, FB_HEIGHT - 1);
  display_write_multiple_pixles(&LCD_JDI_LPM015M135A->lcdc, (uint8_t *)s_framebuffer, 0, 0,
                                FB_WIDTH - 1, FB_HEIGHT - 1);
  PBL_ASSERTN(xSemaphoreTake(s_display_write, portMAX_DELAY) == pdPASS);

  HAL_LCDC_Enter_LP(&LCD_JDI_LPM015M135A->lcdc);
  if (uccb) uccb();
  xSemaphoreGive(s_dma_update_in_progress_semaphore);
  psleep(50);
  return;
}

void display_pulse_vcom(void) {}

void display_show_splash_screen(void) {
  // The bootloader has already drawn the splash screen for us; nothing to do!
}

void display_show_panic_screen(uint32_t error_code) {}

// Stubs for display offset
void display_set_offset(GPoint offset) { s_disp_offset = offset; }

GPoint display_get_offset(void) { return s_disp_offset; }
