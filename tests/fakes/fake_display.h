/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DISP_BYTES_LINE DISP_COLS / 8
// Bytes_per_line + 1 byte for the line address + 1 byte for a null trailer + 1 optional byte for a write command
#define DISP_DMA_BUFFER_SIZE (DISP_BYTES_LINE + 3)

typedef struct {
  uint8_t address;
  uint8_t* data;
} DisplayRow;

typedef bool(*NextRowCallback)(DisplayRow* row);
typedef void(*UpdateCompleteCallback)(void);

typedef enum {
  DISPLAY_STATE_IDLE,
  DISPLAY_STATE_WRITING
} DisplayState;

typedef struct {
  DisplayState state;
  NextRowCallback get_next_row;
  UpdateCompleteCallback complete;
} DisplayContext;


void display_init(void);

void display_clear(void);

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb);

void display_enter_static(void);

void display_pulse_vcom(void);
