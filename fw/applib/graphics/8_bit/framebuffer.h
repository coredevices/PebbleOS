/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <applib/graphics/gtypes.h>

#define FRAMEBUFFER_BYTES_PER_ROW DISP_COLS
#define FRAMEBUFFER_SIZE_BYTES    DISPLAY_FRAMEBUFFER_BYTES

#ifndef UNITTEST
typedef struct FrameBuffer {
  uint8_t buffer[FRAMEBUFFER_SIZE_BYTES];
  GSize size;       //<! Active size of the framebuffer
  GRect dirty_rect; //<! Smallest rect covering all dirty pixels.
  bool is_dirty;
} FrameBuffer;
#else // UNITTEST
// For unit-tests, the framebuffer buffer is moved to the end of the struct
// with no tail padding to allow for DUMA to catch memory overflows
typedef struct FrameBuffer {
  bool is_dirty;
  GSize size;       //<! Active size of the framebuffer
  GRect dirty_rect; //<! Smallest rect covering all dirty pixels.
  uint8_t buffer[FRAMEBUFFER_SIZE_BYTES];
} FrameBuffer;
_Static_assert(sizeof(FrameBuffer) == offsetof(FrameBuffer, buffer) + FRAMEBUFFER_SIZE_BYTES,
               "FrameBuffer must not have tail padding");
#endif

uint8_t *framebuffer_get_line(FrameBuffer *f, uint16_t y);
