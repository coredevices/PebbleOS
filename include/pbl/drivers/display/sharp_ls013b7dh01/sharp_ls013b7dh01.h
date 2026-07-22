/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "../display.h"

#define DISP_LINE_BYTES (DISP_COLS / 8)
#define DISP_LINE_WORDS (((DISP_COLS - 1) / 32) + 1)

// Bytes_per_line + 1 byte for the line address + 1 byte for a null trailer + 1 optional byte for a write command
#define DISP_DMA_BUFFER_SIZE_BYTES (DISP_LINE_BYTES + 3)
#define DISP_DMA_BUFFER_SIZE_WORDS (DISP_LINE_WORDS + 1)

typedef enum {
  DISPLAY_STATE_IDLE,
  DISPLAY_STATE_WRITING
} DisplayState;

typedef struct {
  DisplayState state;
  NextRowCallback get_next_row;
  UpdateCompleteCallback complete;
} DisplayContext;

