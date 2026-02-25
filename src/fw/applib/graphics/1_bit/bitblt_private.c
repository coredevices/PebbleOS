/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../bitblt_private.h"

#include "applib/app_logging.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_private.h"
#include "applib/graphics/gtypes.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/bitset.h"
#include "util/graphics.h"
#include "util/size.h"

#define MAX_SUPPORTED_PALETTE_ENTRIES 4

// stores transparent masks + color patterns for even+odd scanlines
typedef struct {
  // true, if color entry will be visible on 1bit, false otherwise
  bool transparent_mask[MAX_SUPPORTED_PALETTE_ENTRIES];
  // a 32bit value you can OR into the 1bit destination for each color entry
  uint32_t palette_pattern[MAX_SUPPORTED_PALETTE_ENTRIES];
} RowLookUp;

typedef RowLookUp TwoRowLookUp[2];

T_STATIC void prv_apply_tint_color(GColor *color, GColor tint_color) {
  // tint_color.a is always 0 or 3
  if (tint_color.a != 0) {
    tint_color.a = (*color).a;
    *color = tint_color;
  }
}

T_STATIC void prv_calc_two_row_look_ups(TwoRowLookUp *look_up,
                                       GCompOp compositing_mode,
                                       const GColor8 *palette,
                                       uint8_t num_entries,
                                       GColor tint_color) {
  for (unsigned int palette_index = 0; palette_index < num_entries; palette_index++) {
    GColor color = palette[palette_index];
    // gcolor_get_grayscale will convert any color with an alpha less than 2 to clear
    // alpha should be ignored in the case of GCompOpAssign so the alpha is set to 3
    if (compositing_mode == GCompOpAssign) {
      color.a = 3;
    } else if (compositing_mode == GCompOpTint) {
      prv_apply_tint_color(&color, tint_color);
    } else if (compositing_mode == GCompOpTintLuminance) {
      color = gcolor_tint_using_luminance_and_multiply_alpha(color, tint_color);
    }
    color = gcolor_get_grayscale(color);
    for (unsigned int row_number = 0; row_number < ARRAY_LENGTH(*look_up); row_number++) {
      (*look_up)[row_number].palette_pattern[palette_index] =
        graphics_private_get_1bit_grayscale_pattern(color, row_number);
      (*look_up)[row_number].transparent_mask[palette_index] =
        gcolor_is_transparent(color) ? false : true;
    }
  }
}

void bitblt_bitmap_into_bitmap_tiled_palette_to_1bit(GBitmap* dest_bitmap,
                                                     const GBitmap* src_bitmap, GRect dest_rect,
                                                     GPoint src_origin_offset,
                                                     GCompOp compositing_mode,
                                                     GColor tint_color) {
  if (!src_bitmap->palette) {
    return;
  }
  const int8_t dest_begin_x = (dest_rect.origin.x / 32);
  const uint32_t * const dest_block_x_begin = ((uint32_t *)dest_bitmap->addr) + dest_begin_x;
  const int dest_row_length_words = (dest_bitmap->row_size_bytes / 4);
  // The number of bits between the beginning of dest_block and
  // the beginning of the nearest 32-bit block:
  const uint8_t dest_shift_at_line_begin = (dest_rect.origin.x % 32);

  const int16_t src_begin_x = src_bitmap->bounds.origin.x;
  const int16_t src_begin_y = src_bitmap->bounds.origin.y;
  // The bounds size is relative to the bounds origin, but the offset is within the origin. This
  // means that the end coordinates may need to be adjusted.
  const int16_t src_end_x = grect_get_max_x(&src_bitmap->bounds);
  const int16_t src_end_y = grect_get_max_y(&src_bitmap->bounds);

  // how many 32-bit blocks do we need to bitblt on this row:
  const int16_t dest_end_x = grect_get_max_x(&dest_rect);
  const int16_t dest_y_end = grect_get_max_y(&dest_rect);
  const uint8_t num_dest_blocks_per_row = (dest_end_x / 32) +
    ((dest_end_x % 32) ? 1 : 0) - dest_begin_x;

  const GColor *palette = src_bitmap->palette;
  const uint8_t *src = src_bitmap->addr;
  const uint8_t src_bpp = gbitmap_get_bits_per_pixel(gbitmap_get_format(src_bitmap));
  const uint8_t src_palette_size = 1 << src_bpp;
  PBL_ASSERTN(src_palette_size <= MAX_SUPPORTED_PALETTE_ENTRIES);
  // The bitblt loops:
  int16_t src_y = src_begin_y + src_origin_offset.y;
  int16_t dest_y = dest_rect.origin.y;

  TwoRowLookUp look_ups;
  prv_calc_two_row_look_ups(&look_ups, compositing_mode, palette, src_palette_size, tint_color);

  while (dest_y < dest_y_end) {
    if (src_y >= src_end_y) {
      src_y = src_begin_y;
    }
    uint8_t dest_shift = dest_shift_at_line_begin;

    RowLookUp look_up = look_ups[dest_y % 2];

    int16_t src_x = src_begin_x + src_origin_offset.x;
    int16_t row_bits_left = dest_rect.size.w;
    uint32_t *dest_block = (uint32_t *)dest_block_x_begin + (dest_y * dest_row_length_words);

    const uint32_t *dest_block_end = dest_block + num_dest_blocks_per_row;

    while (dest_block != dest_block_end) {
      uint8_t dest_x = dest_shift;
      while (dest_x < 32 && row_bits_left > 0) {
        if (src_x >= src_end_x) { // Wrap horizontally
          src_x = src_begin_x;
        }
        uint8_t cindex = raw_image_get_value_for_bitdepth(src, src_x, src_y,
                                                          src_bitmap->row_size_bytes, src_bpp);
        uint32_t mask = 0;
        bitset32_update(&mask, dest_x, look_up.transparent_mask[cindex]);

        // This can be optimized by performing actions on the current word all at once
        // instead of iterating through each pixel
        switch (compositing_mode) {
          case GCompOpAssign:
          case GCompOpSet:
          case GCompOpTint:
          case GCompOpTintLuminance:
            *dest_block = (*dest_block & ~mask) | (look_up.palette_pattern[cindex] & mask);
            break;
          default:
            PBL_LOG_DBG("Only the assign, set and tint modes are allowed for palettized bitmaps");
            return;
        }
        dest_x++;
        row_bits_left--;
        src_x++;
      }
      dest_shift = 0;
      dest_block++;
    }
    dest_y++;
    src_y++;
  }
}

void bitblt_bitmap_into_bitmap_tiled(GBitmap* dest_bitmap, const GBitmap* src_bitmap,
                                     GRect dest_rect, GPoint src_origin_offset,
                                     GCompOp compositing_mode, GColor8 tint_color) {
  if (bitblt_compositing_mode_is_noop(compositing_mode, tint_color)) {
    return;
  }

  GBitmapFormat src_fmt = gbitmap_get_format(src_bitmap);
  GBitmapFormat dest_fmt = gbitmap_get_format(dest_bitmap);
  if (dest_fmt != GBitmapFormat1Bit) {
    return;
  }

  switch (src_fmt) {
    case GBitmapFormat1Bit:
      bitblt_bitmap_into_bitmap_tiled_1bit_to_1bit(dest_bitmap, src_bitmap, dest_rect,
                                                   src_origin_offset, compositing_mode, tint_color);
      break;
    case GBitmapFormat1BitPalette:
    case GBitmapFormat2BitPalette:
      bitblt_bitmap_into_bitmap_tiled_palette_to_1bit(dest_bitmap, src_bitmap, dest_rect,
                                                      src_origin_offset, compositing_mode,
                                                      tint_color);
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Only 1 and 2 bit palettized images can be displayed.");
      return;
  }
}
