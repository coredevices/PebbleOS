/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef struct {
  const uint8_t backlight_on_percent; // percent of max possible brightness
} BoardConfig;

static const BoardConfig BOARD_CONFIG = {.backlight_on_percent = 100};
