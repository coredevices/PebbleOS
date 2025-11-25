/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gtypes.h"

void bitblt_bitmap_into_bitmap_tiled(GBitmap* dest_bitmap, const GBitmap* src_bitmap,
                                     GRect dest_rect, GPoint src_origin_offset,
                                     GCompOp compositing_mode, GColor tint_color);

void bitblt_bitmap_into_bitmap(GBitmap* dest_bitmap, const GBitmap* src_bitmap, GPoint dest_offset,
                               GCompOp compositing_mode, GColor tint_color);

bool bitblt_compositing_mode_is_noop(GCompOp compositing_mode, GColor tint_color);
