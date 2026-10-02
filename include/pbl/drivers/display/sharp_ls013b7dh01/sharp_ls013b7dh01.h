/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "../display.h"

/**
 * @defgroup drivers_display_sharp_ls013b7dh01 Sharp LS013B7DH01
 * @ingroup drivers_display
 * @brief @ref drivers_display implementation for the Sharp LS013B7DH01 memory LCD.
 * @{
 */

/** @brief Bytes per display line, at one bit per pixel. */
#define DISP_LINE_BYTES (DISP_COLS / 8)
/** @brief 32-bit words per display line, rounded up. */
#define DISP_LINE_WORDS (((DISP_COLS - 1) / 32) + 1)

/**
 * @brief Bytes needed to send one line.
 *
 * Line data, plus the line address, a null trailer and an optional write command.
 */
#define DISP_DMA_BUFFER_SIZE_BYTES (DISP_LINE_BYTES + 3)
/** @brief 32-bit words needed to send one line. */
#define DISP_DMA_BUFFER_SIZE_WORDS (DISP_LINE_WORDS + 1)

/** @brief Display update state. */
typedef enum {
  /** No update in progress. */
  DISPLAY_STATE_IDLE,
  /** Update in progress. */
  DISPLAY_STATE_WRITING
} DisplayState;

/** @brief Display update context. */
typedef struct {
  /** Update state. */
  DisplayState state;
  /** Row source of the current update. */
  NextRowCallback get_next_row;
  /** Completion callback of the current update. */
  UpdateCompleteCallback complete;
} DisplayContext;

/** @} */
