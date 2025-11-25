/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdbool.h>
#include <stdint.h>

typedef const struct ICE40LPDevice {
  struct {
    SPI_TypeDef *periph;
    uint32_t rcc_bit;
    AfConfig clk;
    AfConfig mosi;
    OutputConfig scs;
  } spi;

  OutputConfig creset;
  InputConfig cdone;
  InputConfig busy;

  bool use_6v6_rail;
} ICE40LPDevice;
