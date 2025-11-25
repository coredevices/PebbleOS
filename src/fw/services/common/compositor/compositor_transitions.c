/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "compositor_transitions.h"
#include "compositor_private.h"

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gdraw_command_transforms.h"
#include "applib/graphics/gpath.h"
#include "applib/graphics/graphics_private_raw.h"
#include "applib/graphics/graphics_private_raw_mask.h"
#include "applib/ui/animation_interpolate.h"
#include "kernel/ui/modals/modal_manager.h"
#include "system/passert.h"
#include "util/math.h"

bool compositor_transition_app_to_app_should_be_skipped(void) {
  // App-to-app compositor transitions should only be visible if there are no opaque modal windows
  return !(modal_manager_get_properties() & ModalProperty_Transparent);
}

typedef struct {
  GDrawCommandProcessor draw_command_processor;
  GContext *ctx;
  GColor stroke_color; // replace red to this
  GColor key_color; // replace this color with overdraw_color
  GColor overdraw_color; // replace key_color with this
  GColor app_fb_key_color; // replace with app framebuffer, use GColorClear to skip
  GPoint framebuffer_offset; // displacement for the app framebuffer when drawing
} CompositorColorReplacementProcessor;

static void prv_compositor_replace_colors_processor(GDrawCommandProcessor *processor,
                                                    GDrawCommand *processed_command,
                                                    size_t processed_command_max_size,
                                                    const GDrawCommandList* list,
                                                    const GDrawCommand *command) {
  CompositorColorReplacementProcessor *p = (CompositorColorReplacementProcessor *)processor;

  // fill with app framebuffer (only if a app_fb_key_color != clear was passed)
  if (!gcolor_is_invisible(p->app_fb_key_color) &&
      gcolor_equal(gdraw_command_get_fill_color(processed_command), p->app_fb_key_color)) {
    gdraw_command_set_hidden(processed_command, true);
    const uint16_t num_points = gdraw_command_get_num_points(processed_command);
    GPoint points[num_points];
    if (sizeof(points) == gdraw_command_copy_points(processed_command, points, sizeof(points))) {
      GPath path = {
        .num_points = num_points,
        .points = points
      };
      gpath_draw_filled_with_cb(p->ctx, &path,
                                compositor_app_framebuffer_fill_callback,
                                &p->framebuffer_offset);
    }
  } else {
    // Original SVGs use Red for the stroke, replace it here
    const GColor8 key_stroke_color = GColorRed;
    gdraw_command_replace_color(processed_command, key_stroke_color, p->stroke_color);
    // replace surrounding color
    gdraw_command_replace_color(processed_command, p->key_color, p->overdraw_color);
  }
}

void compositor_transition_pdcs_animation_update(
    GContext *ctx, GDrawCommandSequence *sequence, uint32_t distance_normalized,
    GColor chroma_key_color, GColor stroke_color, GColor overdraw_color, bool inner,
    const GPoint *framebuffer_offset) {
  if (!sequence) {
    return;
  }

  const uint32_t total_duration = gdraw_command_sequence_get_total_duration(sequence);
  const uint32_t elapsed = interpolate_uint32(distance_normalized, 0, total_duration);
  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, elapsed);

  if (!frame) {
    return;
  }

  // Original SVGs use Islamic Green for the overdraw color, if we are filling the inner ring
  // if it's the outer ring (inner == false) this "key color" is GColorGreen,
  // also we don't render the app's framebuffer in this case
  // it's a bit weird but just they way how these SVGs have been designed
  const GColor8 key_color = inner ? GColorIslamicGreen : GColorGreen;
  CompositorColorReplacementProcessor processor = {
    .draw_command_processor.command = prv_compositor_replace_colors_processor,
    .ctx = ctx,
    .stroke_color = stroke_color,
    .key_color = key_color,
    .overdraw_color = overdraw_color,
    .app_fb_key_color = inner ? chroma_key_color : GColorClear,
  };

  gdraw_command_frame_draw_processed(ctx, sequence, frame, GPointZero,
                                     &processor.draw_command_processor);
}

//! Copy horizontal lines from the app framebuffer to the provided framebuffer
//! This is basically duplicated from prv_assign_horizontal_line_raw() in graphics_private_raw.c
void prv_app_fb_fill_assign_horizontal_line(GContext *ctx, int16_t y, Fixed_S16_3 x1,
                                            Fixed_S16_3 x2, GColor color) {
  PBL_ASSERTN(ctx);
  GBitmap *framebuffer = &ctx->dest_bitmap;
  PBL_ASSERTN(framebuffer->bounds.origin.x == 0 && framebuffer->bounds.origin.y == 0);

  // Clip the line to the bitmap data row's range, taking into account fractions
  const GBitmapDataRowInfo destination_data_row_info = gbitmap_get_data_row_info(framebuffer, y);
  x1.raw_value = MAX(x1.raw_value, destination_data_row_info.min_x << FIXED_S16_3_PRECISION);
  x2.raw_value = MIN(x2.raw_value, destination_data_row_info.max_x << FIXED_S16_3_PRECISION);
  if (x1.integer > x2.integer) {
    return;
  }

  GBitmap app_framebuffer = compositor_get_app_framebuffer_as_bitmap();
  // We only check the destination data rows above (and not also the source data rows) because we
  // assume that both source and destination are framebuffers using the native bitmap format
  PBL_ASSERTN(app_framebuffer.info.format == framebuffer->info.format);
  PBL_ASSERTN(app_framebuffer.data_row_infos == framebuffer->data_row_infos);

  const GBitmapDataRowInfo source_data_row_info = gbitmap_get_data_row_info(&app_framebuffer, y);
  GColor8 *input = (GColor8 *)(source_data_row_info.data + x1.integer);
  GColor8 *output = (GColor8 *)(destination_data_row_info.data + x1.integer);

  // First pixel with blending if fraction is different than 0
  const uint16_t data_row_offset =
    (uint16_t)(destination_data_row_info.data - (uint8_t *)framebuffer->addr);
  if (x1.fraction != 0) {
    graphics_private_raw_blend_color_factor(ctx, output, data_row_offset, *input, x1.integer,
                                            (uint8_t)(FIXED_S16_3_ONE.raw_value - x1.fraction));
    input++;
    output++;
    x1.integer++;
  }

  // Middle pixels
  const int16_t width = x2.integer - x1.integer + 1;
  if (width > 0) {
#if CAPABILITY_HAS_MASKING
    const GDrawMask *mask = ctx->draw_state.draw_mask;
    for (int x = x1.integer; x < x1.integer + width; x++) {
      graphics_private_raw_mask_apply(output, mask, data_row_offset, x1.integer, 1, *input);
      input++;
      output++;
    }
#else
    memcpy((uint8_t *)output, (uint8_t *)input, width);
    input += width;
    output += width;
#endif
  }

  // Last pixel with blending (don't render first AND last pixel if line length is 1)
  if (x2.fraction != 0) {
    graphics_private_raw_blend_color_factor(ctx, output, data_row_offset, *input, x2.integer,
                                            (uint8_t)x2.fraction);
  }
}

//! Copy vertical lines from the app framebuffer to the provided framebuffer
//! This is basically duplicated from prv_assign_vertical_line_raw() in graphics_private_raw.c
void prv_app_fb_fill_assign_vertical_line(GContext *ctx, int16_t x, Fixed_S16_3 y1,
                                          Fixed_S16_3 y2, GColor color) {
  PBL_ASSERTN(ctx);
  GBitmap *framebuffer = &ctx->dest_bitmap;
  PBL_ASSERTN(framebuffer->bounds.origin.x == 0 && framebuffer->bounds.origin.y == 0);

  GBitmap app_framebuffer = compositor_get_app_framebuffer_as_bitmap();
  // We assume that both source and destination are framebuffers using the native bitmap format
  PBL_ASSERTN(app_framebuffer.info.format == framebuffer->info.format);
  PBL_ASSERTN(app_framebuffer.data_row_infos == framebuffer->data_row_infos);

  GBitmapDataRowInfo source_data_row_info = gbitmap_get_data_row_info(&app_framebuffer, y1.integer);
  GColor8 *input = (GColor8 *)(source_data_row_info.data + x);

  GBitmapDataRowInfo destination_data_row_info = gbitmap_get_data_row_info(framebuffer, y1.integer);
  GColor8 *output = (GColor8 *)(destination_data_row_info.data + x);

  // first pixel with blending
  const uint16_t data_row_offset =
    (uint16_t)(destination_data_row_info.data - (uint8_t *)framebuffer->addr);
  if (y1.fraction != 0) {
    // Only draw the pixel if its within the bitmap data row range
    if (WITHIN(x, destination_data_row_info.min_x, destination_data_row_info.max_x)) {
      graphics_private_raw_blend_color_factor(ctx, output, data_row_offset, *input, x,
                                              (uint8_t)(FIXED_S16_3_ONE.raw_value - y1.fraction));
    }
    y1.integer++;
    source_data_row_info = gbitmap_get_data_row_info(&app_framebuffer, y1.integer);
    input = (GColor8 *)(source_data_row_info.data + x);
    destination_data_row_info = gbitmap_get_data_row_info(framebuffer, y1.integer);
    output = (GColor8 *)(destination_data_row_info.data + x);
  }

  // middle pixels
  while (y1.integer <= y2.integer) {
    // Only draw the pixel if its within the bitmap data row range
    if (WITHIN(x, destination_data_row_info.min_x, destination_data_row_info.max_x)) {
#if CAPABILITY_HAS_MASKING
      const GDrawMask *mask = ctx->draw_state.draw_mask;
      graphics_private_raw_mask_apply(output, mask, data_row_offset, x, 1, *input);
#else
      *output = *input;
#endif
    }
    y1.integer++;
    source_data_row_info = gbitmap_get_data_row_info(&app_framebuffer, y1.integer);
    input = (GColor8 *)(source_data_row_info.data + x);
    destination_data_row_info = gbitmap_get_data_row_info(framebuffer, y1.integer);
    output = (GColor8 *)(destination_data_row_info.data + x);
  }

  // last pixel with blending (don't render first *and* last pixel if line length is 1)
  if (y2.fraction != 0) {
    // Only draw the pixel if its within the bitmap data row range
    if (WITHIN(x, destination_data_row_info.min_x, destination_data_row_info.max_x)) {
      graphics_private_raw_blend_color_factor(ctx, output, data_row_offset, *input, x,
                                              (uint8_t)y2.fraction);
    }
  }
}

const GDrawRawImplementation g_compositor_transitions_app_fb_draw_implementation = {
  .assign_horizontal_line = prv_app_fb_fill_assign_horizontal_line,
  .assign_vertical_line = prv_app_fb_fill_assign_vertical_line,
  // If you ever experience a crash during compositor transitions (e.g. in an integration tests)
  // then it's likely that you need to provide additional draw handlers here
};
