/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/framebuffer.h"
#include "util/attributes.h"

volatile const int FrameBuffer_MaxX = DISP_COLS;
volatile const int FrameBuffer_MaxY = DISP_ROWS;

void WEAK framebuffer_mark_dirty_rect(FrameBuffer *f, GRect rect) {}

void WEAK framebuffer_init(FrameBuffer *f, const GSize *size) { f->size = *size; }

GSize WEAK framebuffer_get_size(FrameBuffer *f) { return f->size; }
