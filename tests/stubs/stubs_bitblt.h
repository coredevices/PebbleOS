/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"

WEAK void bitblt_bitmap_into_bitmap(GBitmap* dest_bitmap, const GBitmap* src_bitmap,
                                    GPoint dest_offset, GCompOp compositing_mode,
                                    GColor tint_color) {}

WEAK void bitblt_bitmap_into_bitmap_tiled(GBitmap* dest_bitmap, const GBitmap* src_bitmap,
                                          GRect dest_rect, GPoint src_origin_offset,
                                          GCompOp compositing_mode, GColor8 tint_color) {}
