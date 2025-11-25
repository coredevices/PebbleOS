/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "../display.h"

typedef enum {
  DISP_COLOR_BLACK = 0,
  DISP_COLOR_WHITE,
  DISP_COLOR_RED,
  DISP_COLOR_GREEN,
  DISP_COLOR_BLUE,
  DISP_COLOR_MAX
} DispColor;

static const uint8_t s_display_colors[DISP_COLOR_MAX] = { 0x00, 0xff, 0xc0, 0x30, 0x0c };

void display_fill_color(uint8_t color_value);
void display_fill_stripes();
