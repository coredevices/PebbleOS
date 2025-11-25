/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "board/board.h"

#include "bf0_hal.h"
#include "bf0_hal_lcdc.h"

typedef struct DisplayJDIState {
  LCDC_HandleTypeDef hlcdc;
} DisplayJDIState;

typedef struct DisplayJDISplash {
  const uint8_t *data;
  uint16_t width;
  uint16_t height;
} DisplayJDISplash;

typedef const struct DisplayJDIDevice {
  DisplayJDIState *state;
  IRQn_Type irqn;
  uint8_t irq_priority;
  struct {
    LPTIM_TypeDef *lptim;
    uint8_t freq_hz;
  } vcom;
  struct {
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
  } pinmux;
  OutputConfig vddp;
  OutputConfig vlcd;
  DisplayJDISplash splash;
} DisplayJDIDevice;

void display_jdi_irq_handler(DisplayJDIDevice *disp);
