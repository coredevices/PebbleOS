#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../display.h"

typedef struct {
  Pinmux xrst;
  Pinmux vst;
  Pinmux vck;
  Pinmux enb;
  Pinmux hst;
  Pinmux hck;
  Pinmux r1;
  Pinmux r2;
  Pinmux g1;
  Pinmux g2;
  Pinmux b1;
  Pinmux b2;
  Pinmux vcom;
  Pinmux va;
  Pinmux vb;

} LCDPinmux;

typedef struct LCDDevice {
  LCDC_HandleTypeDef lcdc;
  LCDPinmux pin;
} LCDDevice;
void lcd_irq_handler(LCDDevice* lcd);
