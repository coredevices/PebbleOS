/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_private.h"
#include "applib/graphics/graphics_private_raw.h"
#include "applib/graphics/graphics_private_raw_mask.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/framebuffer.h"
#include "applib/ui/window_private.h"
#include "applib/ui/layer.h"
#include "util/graphics.h"
#include "util/trig.h"

#include "clar.h"
#include "util.h"

#include <stdio.h>

// Helper Includes
////////////////////////////////////
#include "test_graphics.h"
#include "test_graphics_mask.h"
#include "8bit/test_framebuffer.h"

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup and Teardown
////////////////////////////////////////////////////////////////////////////////////////////////////

static FrameBuffer *s_fb = NULL;
static GContext *s_ctx = NULL;
static GBitmap *s_dest_bitmap = NULL;

// Setup
void test_graphics_context_mask__initialize(void) {
  s_fb = malloc(sizeof(*s_fb));
  s_ctx = malloc(sizeof(*s_ctx));

  framebuffer_init(s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  test_graphics_context_init(s_ctx, s_fb);
  framebuffer_clear(s_fb);
}

void test_graphics_context_mask__cleanup(void) {
  free(s_ctx);
  free(s_fb);

  gbitmap_destroy(s_dest_bitmap);
  s_dest_bitmap = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
//          COMMON HELPERS           //
///////////////////////////////////////

#define CHECK_EXPECTED_TEST_IMAGE(ctx) (cl_check(gbitmap_pbi_eq(&ctx->dest_bitmap, TEST_PBI_FILE)))

static const int16_t num_pixels_per_mask_value = 5;
static const int16_t num_mask_values = 4;
static const int16_t num_src_colors = 256; // 8-bit color
static const int16_t num_dest_colors = 64; // Framebuffer ignores alpha, so only 6-bit color

static void prv_prepare_canvas(GContext *ctx, GSize desired_size) {
  s_dest_bitmap = gbitmap_create_blank(desired_size, GBitmapFormat8Bit);

  ctx->dest_bitmap = *s_dest_bitmap;
  ctx->draw_state.clip_box.size = desired_size;
  ctx->draw_state.drawing_box.size = desired_size;
}

///////////////////////////////////////
// RECORDING HORIZONTAL LINE HELPERS //
///////////////////////////////////////

static void prv_prepare_canvas_for_hline_recording_test(GContext *ctx) {
  const GSize bitmap_size = GSize(num_mask_values * num_pixels_per_mask_value, num_src_colors);
  prv_prepare_canvas(ctx, bitmap_size);
}

static GDrawMask *prv_create_mask_for_hline_recording_test(GContext *ctx) {
  // The initial transparency doesn't really matter since we're about to overwrite it
  const bool transparent = false;
  GDrawMask *mask = graphics_context_mask_create(ctx, transparent);
  cl_assert(mask);

  const GSize mask_size = ctx->dest_bitmap.bounds.size;
  for (int16_t x = 0; x < mask_size.w; x++) {
    const uint8_t mask_pixel_value = (uint8_t)(x / num_pixels_per_mask_value);
    for (int16_t y = 0; y < mask_size.h; y++) {
      test_graphics_context_mask_set_value_for_coordinate(ctx, mask, mask_pixel_value,
                                                          GPoint(x, y));
    }
  }

  return mask;
}

typedef void (*HorizontalClippingMaskRecordFunc)(GContext *ctx, int16_t y, int16_t x1, int16_t x2,
                                                 GColor color);

static void prv_mask_record_hline_test_pattern(HorizontalClippingMaskRecordFunc record_func) {
  GContext *ctx = s_ctx;

  prv_prepare_canvas_for_hline_recording_test(ctx);

  graphics_context_set_antialiased(ctx, true);

  GDrawMask *mask = prv_create_mask_for_hline_recording_test(ctx);

  cl_assert(graphics_context_mask_record(ctx, mask));

  for (int16_t y = 0; y < num_src_colors; y++) {
    GColor src_color = (GColor) { .argb = (uint8_t)y };
    for (int mask_value_index = 0; mask_value_index < num_mask_values; mask_value_index++) {
      int16_t x1 = (int16_t)(mask_value_index * num_pixels_per_mask_value);
      int16_t x2 = (int16_t)(x1 + num_pixels_per_mask_value - 1);
      record_func(ctx, y, x1, x2, src_color);
    }
  }

  cl_assert(graphics_context_mask_record(ctx, NULL));

  test_graphics_context_mask_debug(ctx, mask);

  graphics_context_mask_destroy(ctx, mask);
}

///////////////////////////////////////
//  RECORDING VERTICAL LINE HELPERS  //
///////////////////////////////////////

static void prv_prepare_canvas_for_vline_recording_test(GContext *ctx) {
  const GSize bitmap_size = GSize(num_src_colors, num_mask_values * num_pixels_per_mask_value);
  prv_prepare_canvas(ctx, bitmap_size);
}

static GDrawMask *prv_create_mask_for_vline_recording_test(GContext *ctx) {
  // The initial transparency doesn't really matter since we're about to overwrite it
  const bool transparent = false;
  GDrawMask *mask = graphics_context_mask_create(ctx, transparent);
  cl_assert(mask);

  const GSize mask_size = ctx->dest_bitmap.bounds.size;
  for (int16_t y = 0; y < mask_size.h; y++) {
    const uint8_t mask_pixel_value = (uint8_t)(y / num_pixels_per_mask_value);
    for (int16_t x = 0; x < mask_size.w; x++) {
      test_graphics_context_mask_set_value_for_coordinate(ctx, mask, mask_pixel_value,
                                                          GPoint(x, y));
    }
  }

  return mask;
}

typedef void (*VerticalClippingMaskRecordFunc)(GContext *ctx, int16_t x, int16_t y1, int16_t y2,
                                               GColor color);

static void prv_mask_record_vline_test_pattern(VerticalClippingMaskRecordFunc record_func) {
  GContext *ctx = s_ctx;

  prv_prepare_canvas_for_vline_recording_test(ctx);

  graphics_context_set_antialiased(ctx, true);

  GDrawMask *mask = prv_create_mask_for_vline_recording_test(ctx);

  cl_assert(graphics_context_mask_record(ctx, mask));

  for (int16_t x = 0; x < num_src_colors; x++) {
    GColor src_color = (GColor) { .argb = (uint8_t)x };
    for (int mask_value_index = 0; mask_value_index < num_mask_values; mask_value_index++) {
      int16_t y1 = (int16_t)(mask_value_index * num_pixels_per_mask_value);
      int16_t y2 = (int16_t)(y1 + num_pixels_per_mask_value - 1);
      record_func(ctx, x, y1, y2, src_color);
    }
  }

  cl_assert(graphics_context_mask_record(ctx, NULL));

  test_graphics_context_mask_debug(ctx, mask);

  graphics_context_mask_destroy(ctx, mask);
}

///////////////////////////////////////
//  APPLYING HORIZONTAL LINE HELPERS //
///////////////////////////////////////

static const int16_t hline_applying_test_column_width = num_mask_values * num_pixels_per_mask_value;

static void prv_prepare_canvas_for_hline_applying_test(GContext *ctx) {
  const GSize bitmap_size = GSize(num_dest_colors * hline_applying_test_column_width,
                                  num_src_colors);
  prv_prepare_canvas(ctx, bitmap_size);

  // Fill the canvas so each column (of width hline_applying_test_column_width) is set to one of the
  // different possible dest_colors
  for (int16_t y = 0; y < bitmap_size.h; y++) {
    for (int16_t column_index = 0; column_index < num_dest_colors; column_index++) {
      const int16_t starting_x = column_index * hline_applying_test_column_width;
      for (int16_t x = starting_x; x < starting_x + hline_applying_test_column_width; x++) {
        ctx->draw_state.stroke_color = (GColor) { .argb = (uint8_t)(column_index | 0b11000000) };
        graphics_draw_pixel(ctx, GPoint(x, y));
      }
    }
  }
}

static GDrawMask *prv_create_mask_for_hline_applying_test(GContext *ctx) {
  // The initial transparency doesn't really matter since we're about to overwrite it
  const bool transparent = false;
  GDrawMask *mask = graphics_context_mask_create(ctx, transparent);
  cl_assert(mask);

  const GSize mask_size = ctx->dest_bitmap.bounds.size;
  for (int16_t x = 0; x < mask_size.w; x++) {
    const uint8_t mask_pixel_value = (uint8_t)((x % hline_applying_test_column_width) /
                                                  num_pixels_per_mask_value);
    for (int16_t y = 0; y < mask_size.h; y++) {
      test_graphics_context_mask_set_value_for_coordinate(ctx, mask, mask_pixel_value,
                                                          GPoint(x, y));
    }
  }

  return mask;
}

typedef void (*HorizontalClippingMaskApplyFunc)(GContext *ctx, int16_t y, int16_t x1, int16_t x2,
                                                GColor color);

static void prv_mask_apply_hline_test_pattern(HorizontalClippingMaskApplyFunc apply_func) {
  GContext *ctx = s_ctx;

  prv_prepare_canvas_for_hline_applying_test(ctx);

  graphics_context_set_antialiased(ctx, true);

  GDrawMask *mask = prv_create_mask_for_hline_applying_test(ctx);

  cl_assert(graphics_context_mask_use(ctx, mask));

  for (int16_t y = 0; y < num_src_colors; y++) {
    GColor src_color = (GColor) { .argb = (uint8_t)y };
    for (int dest_color_index = 0; dest_color_index < num_dest_colors; dest_color_index++) {
      for (int mask_value_index = 0; mask_value_index < num_mask_values; mask_value_index++) {
        int16_t x1 = (int16_t)((dest_color_index * hline_applying_test_column_width) +
                        (mask_value_index * num_pixels_per_mask_value));
        int16_t x2 = (int16_t)(x1 + num_pixels_per_mask_value - 1);
        apply_func(ctx, y, x1, x2, src_color);
      }
    }
  }

  graphics_context_mask_destroy(ctx, mask);
}

//////////////////////////////////////
//  APPLYING VERTICAL LINE HELPERS  //
//////////////////////////////////////

static const int16_t vline_applying_test_row_height = hline_applying_test_column_width;

static void prv_prepare_canvas_for_vline_applying_test(GContext *ctx) {
  const GSize bitmap_size = GSize(num_src_colors,
                                  num_dest_colors * hline_applying_test_column_width);
  prv_prepare_canvas(ctx, bitmap_size);

  // Fill the canvas so each row (of width vline_applying_test_row_height) is set to one of the
  // different possible dest_colors
  for (int16_t x = 0; x < bitmap_size.w; x++) {
    for (int16_t row_index = 0; row_index < num_dest_colors; row_index++) {
      const int16_t starting_y = row_index * vline_applying_test_row_height;
      for (int16_t y = starting_y; y < starting_y + vline_applying_test_row_height; y++) {
        ctx->draw_state.stroke_color = (GColor) { .argb = (uint8_t)(row_index | 0b11000000) };
        graphics_draw_pixel(ctx, GPoint(x, y));
      }
    }
  }
}

static GDrawMask *prv_create_mask_for_vline_applying_test(GContext *ctx) {
  // The initial transparency doesn't really matter since we're about to overwrite it
  const bool transparent = false;
  GDrawMask *mask = graphics_context_mask_create(ctx, transparent);
  cl_assert(mask);

  const GSize mask_size = ctx->dest_bitmap.bounds.size;
  for (int16_t y = 0; y < mask_size.h; y++) {
    const uint8_t mask_pixel_value = (uint8_t)((y % vline_applying_test_row_height) /
      num_pixels_per_mask_value);
    for (int16_t x = 0; x < mask_size.w; x++) {
      test_graphics_context_mask_set_value_for_coordinate(ctx, mask, mask_pixel_value,
                                                          GPoint(x, y));
    }
  }

  return mask;
}

typedef void (*VerticalClippingMaskApplyFunc)(GContext *ctx, int16_t x, int16_t y1, int16_t y2,
                                              GColor color);

static void prv_mask_apply_vline_test_pattern(VerticalClippingMaskApplyFunc apply_func) {
  GContext *ctx = s_ctx;

  prv_prepare_canvas_for_vline_applying_test(ctx);

  graphics_context_set_antialiased(ctx, true);

  GDrawMask *mask = prv_create_mask_for_vline_applying_test(ctx);

  cl_assert(graphics_context_mask_use(ctx, mask));

  for (int16_t x = 0; x < num_src_colors; x++) {
    GColor src_color = (GColor) { .argb = (uint8_t)x };
    for (int dest_color_index = 0; dest_color_index < num_dest_colors; dest_color_index++) {
      for (int mask_value_index = 0; mask_value_index < num_mask_values; mask_value_index++) {
        int16_t y1 = (int16_t)((dest_color_index * vline_applying_test_row_height) +
          (mask_value_index * num_pixels_per_mask_value));
        int16_t y2 = (int16_t)(y1 + num_pixels_per_mask_value - 1);
        apply_func(ctx, x, y1, y2, src_color);
      }
    }
  }

  graphics_context_mask_destroy(ctx, mask);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////
// RECORDING HORIZONTAL LINE TESTS //
/////////////////////////////////////

// These tests initialize a mask that has 4 columns, each of width `num_pixels_per_mask_value`.
// Each of these columns in the mask is initialized to have one of the 4 possible mask values. Then,
// the test iterates over the mask's 256 rows, each of which uses a different src color
// (256 possibilities for 8-bit color) to draw a horizontal line (using the test's specified raw
// drawing function) for each column into the mask (recording).

void prv_mask_recording_assign_horizontal_line(GContext *ctx, int16_t y, Fixed_S16_3 x1,
                                               Fixed_S16_3 x2, GColor color);

static void prv_hline_pattern_record_assign_horizontal_line_raw(GContext *ctx, int16_t y,
                                                                int16_t x1, int16_t x2,
                                                                GColor color) {
  // x1 and x2 here will be the integer start/end of the line, so we need to adjust them so we see
  // the same blending on the first and last pixel
  x2--;
  const Fixed_S16_3 x1_fixed = (Fixed_S16_3) {
    .integer = x1,
    .fraction = 4,
  };
  const Fixed_S16_3 x2_fixed = (Fixed_S16_3) {
    .integer = x2,
    .fraction = 4,
  };
  prv_mask_recording_assign_horizontal_line(ctx, y, x1_fixed, x2_fixed, color);
}

void test_graphics_context_mask__record_assign_horizontal_line_raw(void) {
  prv_mask_record_hline_test_pattern(prv_hline_pattern_record_assign_horizontal_line_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

void prv_mask_recording_blend_horizontal_line_raw(GContext *ctx, int16_t y, int16_t x1,
                                                  int16_t x2, GColor color);

void test_graphics_context_mask__record_blend_horizontal_line_raw(void) {
  prv_mask_record_hline_test_pattern(prv_mask_recording_blend_horizontal_line_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

void prv_mask_recording_assign_horizontal_line_delta_raw(GContext *ctx, int16_t y,
                                                         Fixed_S16_3 x1, Fixed_S16_3 x2,
                                                         uint8_t left_aa_offset,
                                                         uint8_t right_aa_offset,
                                                         int16_t clip_box_min_x,
                                                         int16_t clip_box_max_x,
                                                         GColor color);

static void prv_hline_pattern_record_assign_horizontal_line_delta_raw(GContext *ctx, int16_t y,
                                                                      int16_t x1, int16_t x2,
                                                                      GColor color) {
  const Fixed_S16_3 x1_fixed = (Fixed_S16_3) { .integer = x1 };
  Fixed_S16_3 x2_fixed = (Fixed_S16_3) { .integer = x2 };
  const uint8_t gradient_width = (uint8_t)((x2 - x1) / 2);
  x2_fixed.integer -= gradient_width;

  const int16_t clip_box_min_x = ctx->draw_state.clip_box.origin.x;
  const int16_t clip_box_max_x = (int16_t)(grect_get_max_x(&ctx->draw_state.clip_box) - 1);
  prv_mask_recording_assign_horizontal_line_delta_raw(ctx, y, x1_fixed, x2_fixed, gradient_width,
                                                      gradient_width, clip_box_min_x,
                                                      clip_box_max_x, color);
}

void test_graphics_context_mask__record_assign_horizontal_line_delta_raw(void) {
  prv_mask_record_hline_test_pattern(prv_hline_pattern_record_assign_horizontal_line_delta_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

/////////////////////////////////////
//  RECORDING VERTICAL LINE TESTS  //
/////////////////////////////////////

// These tests initialize a mask that has 4 rows, each of height `num_pixels_per_mask_value`.
// Each of these rows in the mask is initialized to have one of the 4 possible mask values. Then,
// the test iterates over the mask's 256 columns, each of which uses a different src color
// (256 possibilities for 8-bit color) to draw a vertical line (using the test's specified raw
// drawing function) for each row into the mask (recording).

void prv_mask_recording_assign_vertical_line(GContext *ctx, int16_t x, Fixed_S16_3 y1,
                                             Fixed_S16_3 y2, GColor color);

static void prv_vline_pattern_record_assign_vertical_line(GContext *ctx, int16_t x, int16_t y1,
                                                          int16_t y2, GColor color) {
  // y1 and y2 here will be the integer start/end of the line, so we need to adjust them so we see
  // the same blending on the first and last pixel
  y2--;
  const Fixed_S16_3 y1_fixed = (Fixed_S16_3) {
    .integer = y1,
    .fraction = 4,
  };
  const Fixed_S16_3 y2_fixed = (Fixed_S16_3) {
    .integer = y2,
    .fraction = 4,
  };
  prv_mask_recording_assign_vertical_line(ctx, x, y1_fixed, y2_fixed, color);
}

void test_graphics_context_mask__record_assign_vertical_line_raw(void) {
  prv_mask_record_vline_test_pattern(prv_vline_pattern_record_assign_vertical_line);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

void prv_mask_recording_blend_vertical_line_raw(GContext *ctx, int16_t x, int16_t y1, int16_t y2,
                                                GColor color);

void test_graphics_context_mask__record_blend_vertical_line_raw(void) {
  prv_mask_record_vline_test_pattern(prv_mask_recording_blend_vertical_line_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

/////////////////////////////////////
// APPLYING HORIZONTAL LINE TESTS //
/////////////////////////////////////

// These tests initialize a mask that has 4 * 64 columns, each of width `num_pixels_per_mask_value`.
// Each quad grouping of these columns in the mask is initialized to have one of the 4 possible mask
// values. Additionally, the test initializes the framebuffer to have 64 columns, each of width
// `4 * num_pixels_per_mask_value`, where each column is one of the 64 possible dest_colors (the
// framebuffer ignores alpha so 6-bit color). Then, the test activates the mask and iterates over
// the framebuffer's 256 rows, each of which uses a different src color
// (256 possibilities for 8-bit color) to draw a horizontal line (using the test's specified raw
// drawing function) for each `num_pixels_per_mask_value`-wide column.

void prv_assign_horizontal_line_raw(GContext *ctx, int16_t y, Fixed_S16_3 x1, Fixed_S16_3 x2,
                                    GColor color);

static void prv_hline_pattern_apply_assign_horizontal_line_raw(GContext *ctx, int16_t y,
                                                               int16_t x1, int16_t x2,
                                                               GColor color) {
  // x1 and x2 here will be the integer start/end of the line, so we need to adjust them so we see
  // the same blending on the first and last pixel
  x2--;
  const Fixed_S16_3 x1_fixed = (Fixed_S16_3) {
    .integer = x1,
    .fraction = 4,
  };
  const Fixed_S16_3 x2_fixed = (Fixed_S16_3) {
    .integer = x2,
    .fraction = 4,
  };
  prv_assign_horizontal_line_raw(ctx, y, x1_fixed, x2_fixed, color);
}

void test_graphics_context_mask__apply_assign_horizontal_line_raw(void) {
  prv_mask_apply_hline_test_pattern(prv_hline_pattern_apply_assign_horizontal_line_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

void prv_blend_horizontal_line_raw(GContext *ctx, int16_t y, int16_t x1, int16_t x2,
                                   GColor color);

void test_graphics_context_mask__apply_blend_horizontal_line_raw(void) {
  prv_mask_apply_hline_test_pattern(prv_blend_horizontal_line_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

void prv_assign_horizontal_line_delta_raw(GContext *ctx, int16_t y,
                                          Fixed_S16_3 x1, Fixed_S16_3 x2,
                                          uint8_t left_aa_offset, uint8_t right_aa_offset,
                                          int16_t clip_box_min_x, int16_t clip_box_max_x,
                                          GColor color);

static void prv_hline_pattern_apply_assign_horizontal_line_delta_raw(GContext *ctx, int16_t y,
                                                                     int16_t x1, int16_t x2,
                                                                     GColor color) {
  // FIXME PBL-34552: This test produces an incorrect image, see JIRA
  const Fixed_S16_3 x1_fixed = (Fixed_S16_3) { .integer = x1 };
  Fixed_S16_3 x2_fixed = (Fixed_S16_3) { .integer = x2 };
  const uint8_t gradient_width = (uint8_t)((x2 - x1) / 2);
  x2_fixed.integer -= gradient_width;

  const int16_t clip_box_min_x = ctx->draw_state.clip_box.origin.x;
  const int16_t clip_box_max_x = (int16_t)(grect_get_max_x(&ctx->draw_state.clip_box) - 1);
  prv_assign_horizontal_line_delta_raw(ctx, y, x1_fixed, x2_fixed, gradient_width, gradient_width,
                                       clip_box_min_x, clip_box_max_x, color);
}

void test_graphics_context_mask__apply_assign_horizontal_line_delta_raw(void) {
  prv_mask_apply_hline_test_pattern(prv_hline_pattern_apply_assign_horizontal_line_delta_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

//////////////////////////////////
// APPLYING VERTICAL LINE TESTS //
//////////////////////////////////

// These tests initialize a mask that has 4 * 64 rows, each of height
// `vline_applying_test_row_height`. Each quad grouping of these rows in the mask is initialized to
// have one of the 4 possible mask values. Additionally, the test initializes the framebuffer to
// have 64 rows, each of height `4 * num_pixels_per_mask_value`, where each row is one of the 64
// possible dest_colors (the framebuffer ignores alpha so 6-bit color). Then, the test activates the
// mask and iterates over the framebuffer's 256 columns, each of which uses a different src color
// (256 possibilities for 8-bit color) to draw a vertical line (using the test's specified raw
// drawing function) for each `num_pixels_per_mask_value`-tall row.

void prv_assign_vertical_line_raw(GContext *ctx, int16_t x, Fixed_S16_3 y1, Fixed_S16_3 y2,
                                  GColor color);

static void prv_vline_pattern_apply_assign_vertical_line(GContext *ctx, int16_t x, int16_t y1,
                                                         int16_t y2, GColor color) {
  // y1 and y2 here will be the integer start/end of the line, so we need to adjust them so we see
  // the same blending on the first and last pixel
  y2--;
  const Fixed_S16_3 y1_fixed = (Fixed_S16_3) {
    .integer = y1,
    .fraction = 4,
  };
  const Fixed_S16_3 y2_fixed = (Fixed_S16_3) {
    .integer = y2,
    .fraction = 4,
  };
  prv_assign_vertical_line_raw(ctx, x, y1_fixed, y2_fixed, color);
}

void test_graphics_context_mask__apply_assign_vertical_line_raw(void) {
  prv_mask_apply_vline_test_pattern(prv_vline_pattern_apply_assign_vertical_line);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

void prv_blend_vertical_line_raw(GContext *ctx, int16_t x, int16_t y1, int16_t y2, GColor color);

void test_graphics_context_mask__apply_blend_vertical_line_raw(void) {
  // FIXME PBL-34141: This test produces an incorrect image, see JIRA
  prv_mask_apply_vline_test_pattern(prv_blend_vertical_line_raw);
  CHECK_EXPECTED_TEST_IMAGE(s_ctx);
};

/////////////////
// BASIC TESTS //
/////////////////

// These tests test the basic functionality of the clipping mask API.

static void prv_verify_mask_pixel_values(const GContext *ctx, const GDrawMask *mask,
                                         uint8_t expected_value) {
  // Assumes rectangular framebuffer
  cl_assert(ctx->dest_bitmap.info.format == GBitmapFormat8Bit);

  const GSize framebuffer_size = ctx->dest_bitmap.bounds.size;

  for (int16_t x = 0; x < framebuffer_size.w; x++) {
    for (int16_t y = 0; y < framebuffer_size.h; y++) {
      cl_assert_equal_i(test_graphics_context_mask_get_value_for_coordinate(ctx, mask,
                                                                            GPoint(x, y)),
                        expected_value);
    }
  }
}

void test_graphics_context_mask__basic_create(void) {
  GContext *ctx = s_ctx;

  // Should be safe to call create with NULL values
  cl_assert_equal_p(graphics_context_mask_create(NULL, NULL), NULL);

  GDrawMask *transparent_mask = graphics_context_mask_create(ctx, true /* transparent */);
  cl_assert(transparent_mask);
  // Verify all mask pixels are initialized to be transparent (0)
  prv_verify_mask_pixel_values(ctx, transparent_mask, 0);
  graphics_context_mask_destroy(ctx, transparent_mask);

  GDrawMask *opaque_mask = graphics_context_mask_create(ctx, false /* transparent */);
  cl_assert(opaque_mask);
  // Verify all mask pixels are initialized to be opaque (3)
  prv_verify_mask_pixel_values(ctx, opaque_mask, 3);
  graphics_context_mask_destroy(ctx, opaque_mask);
}

//extern const GDrawRawImplementation g_mask_recording_draw_implementation;

void test_graphics_context_mask__basic_record(void) {
  GContext *ctx = s_ctx;

  // Should be safe to call record with NULL values, will return false
  cl_assert(!graphics_context_mask_record(NULL, NULL));

  // Should start with default draw implementation
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_default_draw_implementation);

  GDrawMask *mask1 = graphics_context_mask_create(ctx, true /* transparent */);
  cl_assert(mask1);
  cl_assert(graphics_context_mask_record(ctx, mask1));
  // Should have switched to mask recording draw implementation
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_mask_recording_draw_implementation);
  // Should have attached mask1 to GContext
  cl_assert_equal_p(ctx->draw_state.draw_mask, mask1);

  GDrawMask *mask2 = graphics_context_mask_create(ctx, true /* transparent */);
  cl_assert(mask2);
  cl_assert(graphics_context_mask_record(ctx, mask2));
  // Should still be on mask recording draw implementation
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_mask_recording_draw_implementation);
  // Should now have mask2 attached to GContext
  cl_assert_equal_p(ctx->draw_state.draw_mask, mask2);

  // Calling record with NULL should reset draw impl to default and mask in GContext to NULL
  cl_assert(graphics_context_mask_record(ctx, NULL));
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_default_draw_implementation);
  cl_assert_equal_p(ctx->draw_state.draw_mask, NULL);

  graphics_context_mask_destroy(ctx, mask1);
  graphics_context_mask_destroy(ctx, mask2);
}

void test_graphics_context_mask__basic_use(void) {
  GContext *ctx = s_ctx;

  // Should be safe to call use with NULL values, will return false
  cl_assert(!graphics_context_mask_use(NULL, NULL));

  // Should start with default draw implementation
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_default_draw_implementation);

  GDrawMask *mask1 = graphics_context_mask_create(ctx, true /* transparent */);
  cl_assert(mask1);
  cl_assert(graphics_context_mask_use(ctx, mask1));
  // Should still be on default draw implementation
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_default_draw_implementation);
  // Should have attached mask1 to GContext
  cl_assert_equal_p(ctx->draw_state.draw_mask, mask1);

  GDrawMask *mask2 = graphics_context_mask_create(ctx, true /* transparent */);
  cl_assert(mask2);
  cl_assert(graphics_context_mask_use(ctx, mask2));
  // Should still be on default draw implementation
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_default_draw_implementation);
  // Should now have mask2 attached to GContext
  cl_assert_equal_p(ctx->draw_state.draw_mask, mask2);

  // Calling use with NULL should reset draw impl to default and mask in GContext to NULL
  cl_assert(graphics_context_mask_use(ctx, NULL));
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_default_draw_implementation);
  cl_assert_equal_p(ctx->draw_state.draw_mask, NULL);

  graphics_context_mask_destroy(ctx, mask1);
  graphics_context_mask_destroy(ctx, mask2);
}

void test_graphics_context_mask__basic_destroy(void) {
  // Should be safe to call destroy on NULL values
  graphics_context_mask_destroy(NULL, NULL);
}
