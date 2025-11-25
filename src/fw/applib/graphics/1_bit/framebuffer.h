/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define FRAMEBUFFER_WORDS_PER_ROW ((DISP_COLS / 32) + 1)
#define FRAMEBUFFER_SIZE_DWORDS (DISP_ROWS * FRAMEBUFFER_WORDS_PER_ROW)

#define FRAMEBUFFER_BYTES_PER_ROW (FRAMEBUFFER_WORDS_PER_ROW * 4)
#define FRAMEBUFFER_SIZE_BYTES (DISP_ROWS * FRAMEBUFFER_BYTES_PER_ROW)

typedef struct FrameBuffer {
  uint32_t buffer[FRAMEBUFFER_SIZE_DWORDS];
  GSize size;
  GRect dirty_rect; //<! Smallest rect covering all dirty pixels.
  bool is_dirty;
} FrameBuffer;

uint32_t* framebuffer_get_line(FrameBuffer* f, uint8_t y);
