/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bitblt.h"

void bitblt_bitmap_into_bitmap_tiled_1bit_to_1bit(
    GBitmap* dest_bitmap, const GBitmap* src_bitmap, GRect dest_rect,
    GPoint src_origin_offset, GCompOp compositing_mode, GColor tint_color);

void bitblt_bitmap_into_bitmap_tiled_1bit_to_8bit(
    GBitmap* dest_bitmap, const GBitmap* src_bitmap, GRect dest_rect,
    GPoint src_origin_offset, GCompOp compositing_mode, GColor8 tint_color);

void bitblt_bitmap_into_bitmap_tiled_8bit_to_8bit(
    GBitmap* dest_bitmap, const GBitmap* src_bitmap, GRect dest_rect,
    GPoint src_origin_offset, GCompOp compositing_mode, GColor8 tint_color);

// Used when source bitmap is 1 bit and the destination is 1 or 8 bit.
// Sets up the GCompOp based on the tint_color.
void bitblt_into_1bit_setup_compositing_mode(GCompOp *compositing_mode, GColor tint_color);

extern const GColor8Component g_bitblt_private_blending_mask_lookup[];
