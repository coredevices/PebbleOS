/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gcontext.h"
#include "graphics_private_raw.h"
#include "graphics_private_raw_mask.h"

#include "applib/applib_malloc.auto.h"

#include <string.h>

GDrawMask *graphics_context_mask_create(const GContext *ctx, bool transparent) {
#if CAPABILITY_HAS_MASKING
  if (!ctx) {
    return NULL;
  }

  const GBitmap *framebuffer_bitmap = &ctx->dest_bitmap;
  const int framebuffer_bitmap_height = framebuffer_bitmap->bounds.size.h;

  const size_t num_bytes_needed_for_mask_row_infos =
    sizeof(GDrawMaskRowInfo) * framebuffer_bitmap_height;

  // Iterate over framebuffer data row infos to calculate the Bytes needed for the pixel_mask_data
  size_t num_pixels = 0;
  for (int y = 0; y < framebuffer_bitmap_height; y++) {
    const GBitmapDataRowInfo row_info = gbitmap_get_data_row_info(framebuffer_bitmap, (uint16_t)y);
    const int row_width = row_info.max_x - row_info.min_x + 1;
    num_pixels += row_width;
  }
  // Round up after dividing by the mask bits per pixel
  const size_t num_bytes_needed_for_pixel_mask_data = DIVIDE_CEIL(num_pixels,
                                                                  GDRAWMASK_BITS_PER_PIXEL);

  GDrawMask *result = applib_zalloc(
    sizeof(*result) + num_bytes_needed_for_mask_row_infos + num_bytes_needed_for_pixel_mask_data);
  if (result) {
    // We store the mask_row_infos first in the .data buffer, followed by the pixel_mask_data
    *result = (GDrawMask) {
      .mask_row_infos = (GDrawMaskRowInfo *)result->data,
      .pixel_mask_data = ((uint8_t *)result->data) + num_bytes_needed_for_mask_row_infos,
    };

    // Initialize the mask according to the `transparent` argument
    const uint8_t pixel_data_initial_byte_value = transparent ? (uint8_t)0b00000000 :
                                                  (uint8_t)0b11111111;
    memset(result->pixel_mask_data, pixel_data_initial_byte_value,
           num_bytes_needed_for_pixel_mask_data);

    // Initialize the mask row infos
    const uint16_t fixed_s16_s3_fraction_max_value = FIXED_S16_3_FACTOR - 1;
    for (int y = 0; y < framebuffer_bitmap_height; y++) {
      const GBitmapDataRowInfo row_info = gbitmap_get_data_row_info(framebuffer_bitmap,
                                                                    (uint16_t)y);
      result->mask_row_infos[y] = (GDrawMaskRowInfo) {
        .type = transparent ? GDrawMaskRowInfoType_SemiTransparent : GDrawMaskRowInfoType_Opaque,
        .min_x.integer = row_info.min_x,
        .min_x.fraction = (uint16_t)(transparent ? fixed_s16_s3_fraction_max_value : 0),
        .max_x.integer = row_info.max_x,
        .max_x.fraction = (uint16_t)(transparent ? 0 : fixed_s16_s3_fraction_max_value),
      };
    }
  }

  return result;
#else
  return NULL;
#endif
}

bool graphics_context_mask_record(GContext *ctx, GDrawMask *mask) {
#if CAPABILITY_HAS_MASKING
  if (!ctx) {
    return false;
  }

  if (ctx->draw_state.draw_mask && !mask) {
    // TODO PBL-33766: Update the ctx->draw_state.draw_mask's .mask_row_infos
  }

  const GDrawRawImplementation *draw_implementation_to_set =
    mask ? &g_mask_recording_draw_implementation : &g_default_draw_implementation;

  ctx->draw_state.draw_implementation = draw_implementation_to_set;
  ctx->draw_state.draw_mask = mask;

  return true;
#else
  return false;
#endif
}

bool graphics_context_mask_use(GContext *ctx, GDrawMask *mask) {
#if CAPABILITY_HAS_MASKING
  if (!ctx) {
    return false;
  }

  // Stop any recording
  graphics_context_mask_record(ctx, NULL);

  // If a valid mask is set, the default draw implementation routines will respect it
  ctx->draw_state.draw_mask = mask;

  return true;
#else
  return false;
#endif
}

void graphics_context_mask_destroy(GContext *ctx, GDrawMask *mask) {
#if CAPABILITY_HAS_MASKING
  graphics_context_mask_use(ctx, NULL);
  applib_free(mask);
#endif
}
