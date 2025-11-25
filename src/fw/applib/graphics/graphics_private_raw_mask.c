/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "graphics_private_raw_mask.h"

#include "bitblt_private.h"
#include "gcontext.h"

#include "system/passert.h"
#include "util/graphics.h"

#if CAPABILITY_HAS_MASKING

// Clip the provided fixed x values to the framebuffer's data row info values for the row described
// by y. Return true if clipped values are valid for the row, false otherwise.
static bool prv_clip_fixed_x_values_to_data_row_info(GContext *ctx, int16_t y, Fixed_S16_3 *x1,
                                                     Fixed_S16_3 *x2) {
  const GBitmap *framebuffer = &ctx->dest_bitmap;
  const GBitmapDataRowInfo current_data_row_info = gbitmap_get_data_row_info(framebuffer,
                                                                             (uint16_t)y);
  if (x1 && x2) {
    x1->raw_value = MAX(x1->raw_value, current_data_row_info.min_x << FIXED_S16_3_PRECISION);
    x2->raw_value = MIN(x2->raw_value, current_data_row_info.max_x << FIXED_S16_3_PRECISION);
    return x1->integer <= x2->integer;
  }

  return false;
}

// Clip the provided x values to the values in the provided data row info.
// Return true if clipped values are valid for the row, false otherwise.
static bool prv_clip_x_values_to_data_row_info(const GBitmapDataRowInfo *data_row_info, int16_t *x1,
                                               int16_t *x2) {
  if (data_row_info && x1 && x2) {
    const int16_t clipped_x1 = MAX(*x1, data_row_info->min_x);
    *x1 = clipped_x1;
    const int16_t clipped_x2 = MIN(*x2, data_row_info->max_x);
    *x2 = clipped_x2;
    return clipped_x1 <= clipped_x2;
  }

  return false;
}

static void prv_update_mask(GContext *ctx, int16_t y, int16_t min_x, int16_t max_x,
                            GColor color) {
  PBL_ASSERTN(ctx);

  if (gcolor_is_invisible(color)) {
    return;
  }

  GDrawMask *mask = ctx->draw_state.draw_mask;
  PBL_ASSERTN(mask);

  const GBitmap *framebuffer = &ctx->dest_bitmap;
  const GBitmapDataRowInfo current_data_row_info = gbitmap_get_data_row_info(framebuffer,
                                                                             (uint16_t)y);
  if (!prv_clip_x_values_to_data_row_info(&current_data_row_info, &min_x, &max_x)) {
    return;
  }

  // Update the relevant mask row pixel values
  for (int x = min_x; x <= max_x; x++) {
    const GPoint p = GPoint(x, y);

    // Calculate the new mask pixel value
    const GColor8Component src_color_luminance = gcolor_get_luminance(color);
    const uint8_t current_mask_value = graphics_private_raw_mask_get_value(ctx, mask, p);
    const uint8_t new_pixel_value =
      g_bitblt_private_blending_mask_lookup[(color.a << 4) |
        (current_mask_value << 2) |
        src_color_luminance];

    graphics_private_raw_mask_set_value(ctx, mask, p, new_pixel_value);
  }
}

static void prv_blend_color_and_update_mask(GContext *ctx, int16_t y, int16_t min_x, int16_t max_x,
                                            GColor color, uint8_t factor) {
  color.a = (GColor8Component)(factor * 3 / (FIXED_S16_3_ONE.raw_value - 1));
  prv_update_mask(ctx, y, min_x, max_x, color);
}

T_STATIC void prv_mask_recording_assign_horizontal_line(GContext *ctx, int16_t y,
                                                        Fixed_S16_3 x1, Fixed_S16_3 x2,
                                                        GColor color) {
  if (!prv_clip_fixed_x_values_to_data_row_info(ctx, y, &x1, &x2)) {
    return;
  }

  // first pixel with blending if fraction is different than 0
  if (x1.fraction != 0) {
    prv_blend_color_and_update_mask(ctx, y, x1.integer, x1.integer, color,
                                    (uint8_t)(FIXED_S16_3_ONE.raw_value - x1.fraction));
    x1.integer++;
  }

  // middle pixels
  int16_t last_pixel_x = x2.integer;
  if (x1.integer < x2.integer + 1) {
    prv_update_mask(ctx, y, x1.integer, x2.integer, color);
    // increment the last_pixel since we had some middle pixels
    last_pixel_x++;
    // x1 doesn't need to be increased as it's not used anymore in this function
  }

  // last pixel with blending (don't render first *and* last pixel if line length is 1)
  if (x2.fraction != 0) {
    prv_blend_color_and_update_mask(ctx, y, last_pixel_x, last_pixel_x, color,
                                    (uint8_t)x2.fraction);
  }
}

T_STATIC void prv_mask_recording_assign_vertical_line(GContext *ctx, int16_t x,
                                                      Fixed_S16_3 y1, Fixed_S16_3 y2,
                                                      GColor color) {
  // first pixel with blending
  if (y1.fraction != 0) {
    prv_blend_color_and_update_mask(ctx, y1.integer, x, x, color,
                                    (uint8_t)(FIXED_S16_3_ONE.raw_value - y1.fraction));
    y1.integer++;
  }

  // middle pixels
  while (y1.integer <= y2.integer) {
    prv_update_mask(ctx, y1.integer, x, x, color);
    y1.integer++;
  }

  // last pixel with blending (don't render first *and* last pixel if line length is 1)
  if (y2.fraction != 0) {
    prv_blend_color_and_update_mask(ctx, y1.integer, x, x, color, (uint8_t)y2.fraction);
  }
}

T_STATIC void prv_mask_recording_blend_horizontal_line_raw(GContext *ctx, int16_t y, int16_t x1,
                                                           int16_t x2, GColor color) {
  prv_update_mask(ctx, y, x1, x2, color);
}

T_STATIC void prv_mask_recording_blend_vertical_line_raw(GContext *ctx, int16_t x, int16_t y1,
                                                         int16_t y2, GColor color) {
  PBL_ASSERTN(ctx);
  GBitmap *framebuffer = &ctx->dest_bitmap;
  for (int16_t i = y1; i <= y2; i++) {
    // Skip over pixels outside the bitmap data row's range
    const GBitmapDataRowInfo data_row_info = gbitmap_get_data_row_info(framebuffer, (uint16_t)i);
    if (!WITHIN(x, data_row_info.min_x, data_row_info.max_x)) {
      continue;
    }
    prv_update_mask(ctx, i, x, x, color);
  }
}

T_STATIC void prv_mask_recording_assign_horizontal_line_delta_raw(GContext *ctx, int16_t y,
                                                                  Fixed_S16_3 x1, Fixed_S16_3 x2,
                                                                  uint8_t left_aa_offset,
                                                                  uint8_t right_aa_offset,
                                                                  int16_t clip_box_min_x,
                                                                  int16_t clip_box_max_x,
                                                                  GColor color) {
  PBL_ASSERTN(ctx);
  GBitmap *framebuffer = &ctx->dest_bitmap;
  PBL_ASSERTN(framebuffer->bounds.origin.x == 0 && framebuffer->bounds.origin.y == 0);

  // Clip the clip box to the bitmap data row's range
  const GBitmapDataRowInfo data_row_info = gbitmap_get_data_row_info(framebuffer, (uint16_t)y);
  clip_box_min_x = MAX(clip_box_min_x, data_row_info.min_x);
  clip_box_max_x = MIN(clip_box_max_x, data_row_info.max_x);
  // If x1 is further outside the clip box than the left gradient width, we need to move x1 up
  // to clip_box_min_x and proceed such that we don't draw the left gradient
  int16_t x1_distance_outside_clip_box = clip_box_min_x - x1.integer;
  if (x1_distance_outside_clip_box > left_aa_offset) {
    left_aa_offset = 0;
    x1.integer += x1_distance_outside_clip_box;
  }

  // Clip x2 to clip_box_max_x
  x2.integer = MIN(clip_box_max_x, x2.integer);

  // Return early if there's nothing to draw
  if (x1.integer > x2.integer) {
    return;
  }

  // first pixel with blending
  if (left_aa_offset == 1) {
    // To prevent bleeding of left-hand AA below clip_box
    if (x1.integer >= clip_box_min_x) {
      prv_blend_color_and_update_mask(ctx, y, x1.integer, x1.integer, color,
                                      (uint8_t)(FIXED_S16_3_ONE.raw_value - x1.fraction));
    }
    x1.integer++;
    // or first AA gradient with blending
  } else {
    for (int i = 0; i < left_aa_offset; i++) {
      // To preserve gradient with clipping:
      if (x1.integer < clip_box_min_x) {
        x1.integer++;
        continue;
      }
      if (x1.integer > clip_box_max_x) {
        break;
      }
      prv_blend_color_and_update_mask(ctx, y, x1.integer, x1.integer, color,
                                      (uint8_t)(FIXED_S16_3_ONE.raw_value * i / left_aa_offset));
      x1.integer++;
    }
  }

  // middle pixels
  if (x1.integer < x2.integer + 1) {
    prv_update_mask(ctx, y, x1.integer, x2.integer, color);
  }

  // last pixel with blending (don't render first *and* last pixel if line length is 1)
  if (right_aa_offset <= 1) {
    if (x1.integer <= clip_box_max_x) {
      prv_blend_color_and_update_mask(ctx, y, x1.integer, x1.integer, color, (uint8_t)x2.fraction);
    }
    // or last AA gradient with blending
  } else {
    for (int i = 0; i < right_aa_offset; i++) {
      if (x1.integer > clip_box_max_x) {
        break;
      }
      prv_blend_color_and_update_mask(ctx, y, x1.integer, x1.integer, color,
                                      (uint8_t)(FIXED_S16_3_ONE.raw_value * (right_aa_offset - i) /
                                        right_aa_offset));
      x1.integer++;
    }
  }
}

const GDrawRawImplementation g_mask_recording_draw_implementation = {
  .assign_horizontal_line = prv_mask_recording_assign_horizontal_line,
  .assign_vertical_line = prv_mask_recording_assign_vertical_line,
  .blend_horizontal_line = prv_mask_recording_blend_horizontal_line_raw,
  .blend_vertical_line = prv_mask_recording_blend_vertical_line_raw,
  .assign_horizontal_line_delta = prv_mask_recording_assign_horizontal_line_delta_raw,
  // If you ever experience a crash while recording/using a mask, then it's likely that you need to
  // provide additional draw handlers here
};

// Lookup table to "multiply" two alpha values
// dst.a = multiplied_alpha[src.a][dst.a];
static const GColor8Component s_multiplied_alpha_lookup[4][4] = {
  {0, 0, 0, 0},
  {0, 0, 1, 1},
  {0, 1, 1, 2},
  {0, 1, 2, 3},
};

void graphics_private_raw_mask_apply(GColor8 *dst_color, const GDrawMask *mask,
                                     unsigned int data_row_offset, int x, int width,
                                     GColor8 src_color) {
  if (!dst_color) {
    return;
  }

  // If there's no mask, just set the color normally and return
  if (!mask) {
    memset(dst_color, src_color.argb, (size_t)width);
    return;
  }

  const uint8_t pixels_per_byte = (uint8_t)GDRAWMASK_PIXELS_PER_BYTE;
  const unsigned int mask_row_data_offset = data_row_offset / pixels_per_byte;
  const uint8_t *mask_row_data = &(((uint8_t *)mask->pixel_mask_data)[mask_row_data_offset]);

  // Use 0 for row_stride_bytes and y since we've already moved the pointer to the row of interest
  const uint16_t row_stride_bytes = 0;
  // We have to adjust x because mask_row_data_offset might not be on a Byte boundary
  const unsigned int x_adjustment = data_row_offset % pixels_per_byte;

  for (int current_x = x; current_x < x + width; current_x++) {
    const uint8_t mask_pixel_value = raw_image_get_value_for_bitdepth(mask_row_data,
                                                                      current_x + x_adjustment,
                                                                      0 /* y */, row_stride_bytes,
                                                                      GDRAWMASK_BITS_PER_PIXEL);
    // Make a copy of src_color and multiply its alpha with the mask pixel value
    GColor8 alpha_adjusted_src_color = src_color;
    alpha_adjusted_src_color.a = s_multiplied_alpha_lookup[mask_pixel_value][src_color.a];

    // Blend alpha_adjusted_src_color with dst_color to produce the final dst_color
    dst_color->argb = gcolor_alpha_blend(alpha_adjusted_src_color, *dst_color).argb;
    dst_color++;
  }
}

ALWAYS_INLINE uint8_t graphics_private_raw_mask_get_value(const GContext *ctx,
                                                          const GDrawMask *mask, GPoint p) {
  const GBitmap *framebuffer_bitmap = &ctx->dest_bitmap;
  const GBitmapDataRowInfo data_row_info = gbitmap_get_data_row_info(framebuffer_bitmap, p.y);
  // Calculate a pointer to the start of the row of interest in the pixel mask data
  const unsigned int data_row_info_offset =
    data_row_info.data - (uint8_t *)framebuffer_bitmap->addr;

  const uint8_t pixels_per_byte = GDRAWMASK_PIXELS_PER_BYTE;
  const unsigned int mask_row_data_offset = data_row_info_offset / pixels_per_byte;
  const uint8_t *mask_row_data = &(((uint8_t *)mask->pixel_mask_data)[mask_row_data_offset]);

  // Use 0 for row_stride_bytes and y since we've already moved the pointer to the row of interest
  const uint16_t row_stride_bytes = 0;
  const uint32_t fake_y = 0;

  // We have to adjust x because mask_row_data_offset might not be on a Byte boundary
  const int adjusted_x = p.x + (data_row_info_offset % pixels_per_byte);
  return raw_image_get_value_for_bitdepth(mask_row_data, adjusted_x, fake_y, row_stride_bytes,
                                          GDRAWMASK_BITS_PER_PIXEL);
}

ALWAYS_INLINE void graphics_private_raw_mask_set_value(const GContext *ctx, GDrawMask *mask,
                                                       GPoint p, uint8_t value) {
  const GBitmap *framebuffer_bitmap = &ctx->dest_bitmap;
  const GBitmapDataRowInfo data_row_info = gbitmap_get_data_row_info(framebuffer_bitmap, p.y);
  // Calculate a pointer to the start of the row of interest in the pixel mask data
  const unsigned int data_row_info_offset =
    data_row_info.data - (uint8_t *)framebuffer_bitmap->addr;

  const uint8_t pixels_per_byte = GDRAWMASK_PIXELS_PER_BYTE;
  const unsigned int mask_row_data_offset = data_row_info_offset / pixels_per_byte;
  uint8_t *mask_row_data = &(((uint8_t *)mask->pixel_mask_data)[mask_row_data_offset]);

  // Use 0 for row_stride_bytes and y since we've already moved the pointer to the row of interest
  const uint16_t row_stride_bytes = 0;
  const uint32_t fake_y = 0;

  // We have to adjust x because mask_row_data_offset might not be on a Byte boundary
  const int adjusted_x = p.x + (data_row_info_offset % pixels_per_byte);
  raw_image_set_value_for_bitdepth(mask_row_data, (uint32_t)adjusted_x, fake_y, row_stride_bytes,
                                   GDRAWMASK_BITS_PER_PIXEL, value);
}

#endif // CAPABILITY_HAS_MASKING
