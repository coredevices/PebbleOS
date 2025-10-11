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

#pragma once

#include <stdint.h>

#include "board/board.h"

#include "bf0_hal.h"
#include "bf0_hal_lcdc.h"

typedef struct DisplayJDIState {
  LCDC_HandleTypeDef hlcdc;
} DisplayJDIState;

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
} DisplayJDIDevice;

void jdi_lpm015m135a_irq_handler(DisplayJDIDevice *disp);
