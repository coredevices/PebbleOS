/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"
#include "applib/ui/bitmap_layer.h"
#include "applib/ui/window_private.h"
#include "util/size.h"

#include <stdio.h>

// Helper Functions
////////////////////////////////////

#include "test_graphics.h"
#include "../graphics/util.h"

#if (SCREEN_COLOR_DEPTH_BITS == 1)
#include "1bit/test_framebuffer.h"
#elif (SCREEN_COLOR_DEPTH_BITS == 8)
#include "8bit/test_framebuffer.h"
#else
#error "Unrecognized SCREEN_COLOR_DEPTH_BITS"
#endif


// Stubs
////////////////////////////////////

#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

// Setup
////////////////////////////////////

static GBitmap test_image;
static GContext s_ctx;
static FrameBuffer *s_fb = NULL;

void test_graphics_draw_bitmap__initialize(void) {
  s_fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  test_graphics_context_init(&s_ctx, s_fb);
  read_pbi("no_litter_crop.png.pbi", &test_image);
}

void test_graphics_draw_bitmap__cleanup(void) {
  free(s_fb);
  free(test_image.addr);
}

// Layout Test Support
////////////////////////////////////

#define TEST_LAYER_SIZE (GSize(64, 110))
#define TEST_LAYER_OFFSET_ORIGIN (GPoint(80, 55))

static GPoint s_layer_test_image_bounds_offset;

static void prv_layer_test_update_proc(Layer *layer, GContext *ctx) {
  GRect destination = test_image.bounds;
  destination.origin = s_layer_test_image_bounds_offset;
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_bitmap_in_rect(ctx, &test_image, &destination);
}

static void prv_layer_test(GPoint layer_origin, GPoint image_bounds_offset) {
  Layer layer;
  const GRect layer_frame = (GRect) {
    .origin = layer_origin,
    .size = TEST_LAYER_SIZE,
  };
  layer_init(&layer, &layer_frame);
  layer_set_update_proc(&layer, prv_layer_test_update_proc);

  s_layer_test_image_bounds_offset = image_bounds_offset;

  layer_render_tree(&layer, &s_ctx);
}

static void prv_origin_layer_test(GPoint image_bounds_offset) {
  prv_layer_test(GPointZero, image_bounds_offset);
}

static void prv_offset_layer_test(GPoint image_bounds_offset) {
  prv_layer_test(TEST_LAYER_OFFSET_ORIGIN, image_bounds_offset);
}

static void prv_bitmap_layer_test(GPoint frame_origin, GPoint bounds_origin) {
  BitmapLayer bitmap_layer;
  const GRect bitmap_layer_frame = (GRect) {
    .origin = frame_origin,
    .size = TEST_LAYER_SIZE,
  };
  bitmap_layer_init(&bitmap_layer, &bitmap_layer_frame);
  Layer *layer = bitmap_layer_get_layer(&bitmap_layer);

  GRect bitmap_layer_bounds = bitmap_layer_frame;
  bitmap_layer_bounds.origin = bounds_origin;
  layer_set_bounds(layer, &bitmap_layer_bounds);

  bitmap_layer_set_bitmap(&bitmap_layer, &test_image);
  bitmap_layer_set_compositing_mode(&bitmap_layer, GCompOpAssign);
  layer_render_tree(bitmap_layer_get_layer(&bitmap_layer), &s_ctx);
}

static void prv_origin_bitmap_layer_test(GPoint bounds_origin) {
  prv_bitmap_layer_test(GPointZero, bounds_origin);
}

static void prv_offset_bitmap_layer_test(GPoint bounds_origin) {
  prv_bitmap_layer_test(TEST_LAYER_OFFSET_ORIGIN, bounds_origin);
}

// Composite Test Support
////////////////////////////////////

static GBitmap *prv_create_bitmap_from_png_file(const char *png_filename_without_extension) {
  GBitmap *result = NULL;

  char png_file_path[strlen(CLAR_FIXTURE_PATH) + 1 + strlen(GRAPHICS_FIXTURE_PATH) + 1 +
    strlen(png_filename_without_extension) + 1];
  sprintf(png_file_path, "%s/%s/%s.png", CLAR_FIXTURE_PATH, GRAPHICS_FIXTURE_PATH,
          png_filename_without_extension);

  FILE *fp = fopen(png_file_path, "r");
  if (fp) {
    // Go to the end of the file to discover the size of the file
    if (fseek(fp, 0, SEEK_END) != 0) {
      goto cleanup;
    }
    const long end_of_file = ftell(fp);
    const size_t file_size = (size_t)end_of_file;
    if (end_of_file == -1) {
      goto cleanup;
    }
    uint8_t *png_data = malloc((size_t)file_size);
    if (!png_data) {
      goto cleanup;
    }

    // Go back to the start of the file and read it into the buffer
    if (fseek(fp, 0L, SEEK_SET) != 0) {
      goto cleanup;
    }
    cl_assert_equal_i(fread(png_data, 1, file_size, fp), file_size);

    // Create a GBitmap using the PNG data
    result = gbitmap_create_from_png_data(png_data, file_size);
    if (result) {
      free(png_data);
    }
  }

  cleanup:
  fclose(fp);

  return result;
}

static GBitmap *prv_create_bitmap_from_pbi_file(const char *pbi_filename_without_extension) {
  // Create a static GBitmap to avoid modifying the read_pbi() function to return a GBitmap
  static GBitmap result;

  char pbi_filename[PATH_STRING_LENGTH] = {0};
  snprintf(pbi_filename, PATH_STRING_LENGTH, "%s.pbi", pbi_filename_without_extension);

  if (!read_pbi(pbi_filename, &result)) {
    return NULL;
  } else {
    return &result;
  }
}

typedef GBitmap *(*CompositeTestGBitmapCreateFunc)(const char *filename);

typedef struct CompositeTest {
  const char *test_name;
  GBitmapFormat expected_test_image_bitmap_format;
  CompositeTestGBitmapCreateFunc bitmap_create_func;
  bool need_to_destroy_bitmap;
} CompositeTest;

static const CompositeTest s_composite_tests[] = {
  {
    .test_name = "1bitBW",
    .expected_test_image_bitmap_format = GBitmapFormat1Bit,
    .bitmap_create_func = prv_create_bitmap_from_pbi_file,
    .need_to_destroy_bitmap = false,
  },
  {
    .test_name = "2bitTrns",
    .expected_test_image_bitmap_format = GBitmapFormat2BitPalette,
    .bitmap_create_func = prv_create_bitmap_from_png_file,
    .need_to_destroy_bitmap = true,
  },
#if PBL_COLOR
  {
    .test_name = "4bitTrns",
    .expected_test_image_bitmap_format = GBitmapFormat4BitPalette,
    .bitmap_create_func = prv_create_bitmap_from_png_file,
    .need_to_destroy_bitmap = true,
  },
  {
    .test_name = "8bitTrns",
    .expected_test_image_bitmap_format = GBitmapFormat8Bit,
    .bitmap_create_func = prv_create_bitmap_from_png_file,
    .need_to_destroy_bitmap = true,
  },
#endif
};

#define COMPOSITE_TEST_IMAGE_SIZE_WIDTH (100)

#define COMPOSITE_TEST_OFFSET_X (COMPOSITE_TEST_IMAGE_SIZE_WIDTH / 2)
#define COMPOSITE_TEST_OFFSET_Y (0)

static bool prv_gbitmap_format_and_compositing_mode_combo_is_valid(GBitmapFormat bitmap_format,
                                                                   GCompOp compositing_mode) {
  return !((bitmap_format != GBitmapFormat1Bit) &&
           ((compositing_mode == GCompOpAssignInverted) ||
           (compositing_mode == GCompOpOr) ||
           (compositing_mode == GCompOpAnd) ||
           (compositing_mode == GCompOpClear)));
}

static void prv_composite_test_draw_bitmap(GContext *ctx, GBitmap *bitmap, GPoint offset,
                                           GCompOp compositing_mode) {
  GRect destination = bitmap->bounds;
  destination.origin = gpoint_add(destination.origin, offset);
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_bitmap_in_rect(ctx, bitmap, &destination);
  destination.origin = gpoint_add(destination.origin, GPoint(COMPOSITE_TEST_OFFSET_X,
                                                             COMPOSITE_TEST_OFFSET_Y));
  graphics_context_set_compositing_mode(ctx, compositing_mode);
  graphics_draw_bitmap_in_rect(ctx, bitmap, &destination);
}

static void prv_composite_test(const char *unit_test_name, GCompOp compositing_mode) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_composite_tests); i++) {
    const CompositeTest *test_data = &s_composite_tests[i];

    // Skip invalid GBitmapFormat and GCompOp combinations
    if (!prv_gbitmap_format_and_compositing_mode_combo_is_valid(
      test_data->expected_test_image_bitmap_format, compositing_mode)) {
      break;
    }

    framebuffer_clear(s_fb);

    char test_image_filename[PATH_STRING_LENGTH] = {0};
    snprintf(test_image_filename, PATH_STRING_LENGTH, "test_graphics_draw_bitmap_%s_test_image",
             test_data->test_name);

    GBitmap *bitmap = test_data->bitmap_create_func(test_image_filename);
    cl_assert(bitmap);
    cl_assert(gbitmap_get_format(bitmap) == test_data->expected_test_image_bitmap_format);

    // Draw the two variations of the test image at GPointZero
    prv_composite_test_draw_bitmap(&s_ctx, bitmap, GPointZero, compositing_mode);

    // Then redraw the two variations offset so the bottom right edge of the right variation
    // is aligned with the bottom right edge of the framebuffer
    const GPoint framebuffer_bottom_right_point =
      GPoint(grect_get_max_x(&s_ctx.dest_bitmap.bounds),
             grect_get_max_y(&s_ctx.dest_bitmap.bounds));
    GPoint offset_point = gpoint_sub(framebuffer_bottom_right_point,
                                     GPoint(bitmap->bounds.size.w, bitmap->bounds.size.h));
    offset_point = gpoint_sub(offset_point, GPoint(COMPOSITE_TEST_OFFSET_X,
                                                   COMPOSITE_TEST_OFFSET_Y));
    prv_composite_test_draw_bitmap(&s_ctx, bitmap, offset_point, compositing_mode);

    // Check the result
    char unit_test_result_image_file_base_name[PATH_STRING_LENGTH] = {0};
    snprintf(unit_test_result_image_file_base_name, PATH_STRING_LENGTH,
             "%s_%s", unit_test_name, test_data->test_name);
    cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap,
               namecat(unit_test_result_image_file_base_name, ".pbi")));

    if (test_data->need_to_destroy_bitmap) {
      gbitmap_destroy(bitmap);
    }
  }
}

// Tests
////////////////////////////////////

void test_graphics_draw_bitmap__origin_layer_inside(void) {
  prv_origin_layer_test(GPointZero);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_layer_across_x(void) {
  prv_origin_layer_test(GPoint(25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_layer_across_nx(void) {
  prv_origin_layer_test(GPoint(-25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_layer_across_y(void) {
  prv_origin_layer_test(GPoint(0, 40));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_layer_across_ny(void) {
  prv_origin_layer_test(GPoint(0, -40));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_bitmap_layer_inside(void) {
  prv_origin_bitmap_layer_test(GPointZero);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_bitmap_layer_across_x(void) {
  prv_origin_bitmap_layer_test(GPoint(25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_bitmap_layer_across_nx(void) {
  prv_origin_bitmap_layer_test(GPoint(-25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_bitmap_layer_across_y(void) {
  prv_origin_bitmap_layer_test(GPoint(0, 75));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__origin_bitmap_layer_across_ny(void) {
  prv_origin_bitmap_layer_test(GPoint(0, -25));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_layer_inside(void) {
  prv_offset_layer_test(GPointZero);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_layer_across_x(void) {
  prv_offset_layer_test(GPoint(25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_layer_across_nx(void) {
  prv_offset_layer_test(GPoint(-25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_layer_across_y(void) {
  prv_offset_layer_test(GPoint(0, 40));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_layer_across_ny(void) {
  prv_offset_layer_test(GPoint(0, -40));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_bitmap_layer_inside(void) {
  prv_offset_bitmap_layer_test(GPointZero);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_bitmap_layer_across_x(void) {
  prv_offset_bitmap_layer_test(GPoint(25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_bitmap_layer_across_nx(void) {
  prv_offset_bitmap_layer_test(GPoint(-25, 0));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_bitmap_layer_across_y(void) {
  prv_offset_bitmap_layer_test(GPoint(0, 75));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__offset_bitmap_layer_across_ny(void) {
  prv_offset_bitmap_layer_test(GPoint(0, -25));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_draw_bitmap__composite_assign(void) {
  prv_composite_test(__func__, GCompOpAssign);
}

void test_graphics_draw_bitmap__composite_assign_inverted(void) {
  prv_composite_test(__func__, GCompOpAssignInverted);
}

void test_graphics_draw_bitmap__composite_or(void) {
  prv_composite_test(__func__, GCompOpOr);
}

void test_graphics_draw_bitmap__composite_and(void) {
  prv_composite_test(__func__, GCompOpAnd);
}

void test_graphics_draw_bitmap__composite_clear(void) {
  prv_composite_test(__func__, GCompOpClear);
}

void test_graphics_draw_bitmap__composite_set(void) {
  prv_composite_test(__func__, GCompOpSet);
}

void test_graphics_draw_bitmap__composite_tint(void) {
  graphics_context_set_tint_color(&s_ctx, GColorOrange);
  prv_composite_test(__func__, GCompOpTint);
}

void test_graphics_draw_bitmap__composite_tint_luminance_black_opaque(void) {
  graphics_context_set_tint_color(&s_ctx, GColorBlack);
  prv_composite_test(__func__, GCompOpTintLuminance);
}

void test_graphics_draw_bitmap__composite_tint_luminance_black_semitransparent(void) {
  GColor tint_color = GColorBlack;
  tint_color.a = 2;
  // We have to set the tint color directly in the GContext because
  // graphics_context_set_tint_color() calls gcolor_closest_opaque() on the color you give it
  s_ctx.draw_state.tint_color = tint_color;
  prv_composite_test(__func__, GCompOpTintLuminance);
}

void test_graphics_draw_bitmap__composite_tint_luminance_blue_opaque(void) {
  graphics_context_set_tint_color(&s_ctx, GColorBlue);
  prv_composite_test(__func__, GCompOpTintLuminance);
}
