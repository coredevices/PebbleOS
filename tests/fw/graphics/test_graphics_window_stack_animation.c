/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_private.h"
#include "applib/graphics/graphics_private_raw.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/framebuffer.h"
#include "applib/ui/window_private.h"
#include "applib/ui/window_stack_animation_round.h"
#include "applib/ui/layer.h"
#include "board/displays/display_spalding.h"
#include "services/common/compositor/compositor_transitions.h"
#include "util/trig.h"

#include "clar.h"
#include "util.h"

#include <stdio.h>

// Helper Functions
////////////////////////////////////
#include "test_graphics.h"
#include "8bit/test_framebuffer.h"

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

void window_transition_context_appearance_call_all(WindowTransitioningContext *ctx) {}

void window_render(Window *window, GContext *ctx) {}

bool compositor_transition_app_to_app_should_be_skipped(void) {
  return false;
}

AnimationPrivate *animation_private_animation_find(Animation *handle) {
  return NULL;
}

AnimationProgress animation_private_get_animation_progress(const AnimationPrivate *animation) {
  return 0;
}

const AnimationImplementation* animation_get_implementation(Animation *animation_h) {
  return NULL;
}

const GDrawRawImplementation g_compositor_transitions_app_fb_draw_implementation = {0};

void compositor_port_hole_transition_draw_outer_ring(GContext *ctx, int16_t pixels,
                                                     GColor ring_color) {
}

// Setup and Teardown
////////////////////////////////////

static FrameBuffer *fb = NULL;

// Setup
void test_graphics_window_stack_animation__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) { DISP_COLS, DISP_ROWS });
}

void test_graphics_window_stack_animation__cleanup(void) {
  free(fb);
}

// Helpers
////////////////////////////////////

typedef void (*ClippingMaskDrawFunc)(GContext *ctx);

static void prv_test_clipping_mask(ClippingMaskDrawFunc draw_func, const char *expected_image) {
  GContext *ctx = malloc(sizeof(GContext));
  test_graphics_context_init(ctx, fb);
  framebuffer_clear(fb);

  graphics_context_set_antialiased(ctx, true);

  // Start by filling the framebuffer with green pixels to make things easier to see
  memset(ctx->dest_bitmap.addr, GColorGreenARGB8, FRAMEBUFFER_SIZE_BYTES);

  const bool transparent = true;
  GDrawMask *mask = graphics_context_mask_create(ctx, transparent);
  cl_assert(mask);

  cl_assert(graphics_context_mask_record(ctx, mask));

  draw_func(ctx);

  cl_assert(graphics_context_mask_record(ctx, NULL));
  cl_assert_equal_p(ctx->draw_state.draw_implementation, &g_default_draw_implementation);

  cl_assert(graphics_context_mask_use(ctx, mask));
  cl_assert_equal_p(ctx->draw_state.draw_mask, mask);

  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_rect(ctx, &ctx->dest_bitmap.bounds);

  cl_assert(graphics_context_mask_use(ctx, NULL));
  cl_assert_equal_p(ctx->draw_state.draw_mask, NULL);

  cl_check(gbitmap_pbi_eq(&ctx->dest_bitmap, TEST_NAMED_PBI_FILE(expected_image)));

  graphics_context_mask_destroy(ctx, mask);

  free(ctx);
}

// Tests
////////////////////////////////////

// NOTE: All of the following tests first fill the framebuffer with green to make it easier to see
// incorrect pixels

// This test records a clipping mask of the first frame of the left "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_left_flip_first_frame_clipping(GContext *ctx) {
  compositor_round_flip_transitions_flip_animation_update(ctx, 0,
                                                          CompositorTransitionDirectionLeft,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__left_flip_first_frame_clipping(void) {
  prv_test_clipping_mask(prv_left_flip_first_frame_clipping, "left_flip_first_frame_clipping");
};

// This test records a clipping mask of the 1/4 progress frame of the left "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_left_flip_first_quarter_frame_clipping(GContext *ctx) {
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX / 4,
                                                          CompositorTransitionDirectionLeft,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__left_flip_first_quarter_frame_clipping(void) {
  prv_test_clipping_mask(prv_left_flip_first_quarter_frame_clipping,
                         "left_flip_first_quarter_frame_clipping");
};

// This test records a clipping mask of the half progress frame of the left "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_left_flip_half_frame_clipping(GContext *ctx) {
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX / 2,
                                                          CompositorTransitionDirectionLeft,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__left_flip_half_frame_clipping(void) {
  prv_test_clipping_mask(prv_left_flip_half_frame_clipping, "left_flip_half_frame_clipping");
};

// This test records a clipping mask of the 3/4 progress frame of the left "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_left_flip_third_quarter_frame_clipping(GContext *ctx) {
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX * 3 / 4,
                                                          CompositorTransitionDirectionLeft,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__left_flip_third_quarter_frame_clipping(void) {
  prv_test_clipping_mask(prv_left_flip_third_quarter_frame_clipping,
                         "left_flip_third_quarter_frame_clipping");
};

// This test records a clipping mask of the last frame of the left "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_left_flip_last_frame_clipping(GContext *ctx) {
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX,
                                                          CompositorTransitionDirectionLeft,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__left_flip_last_frame_clipping(void) {
  prv_test_clipping_mask(prv_left_flip_last_frame_clipping, "left_flip_last_frame_clipping");
};

// This test records a clipping mask of the first frame of the right "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_right_flip_first_frame_clipping(GContext *ctx) {
  // The first frame is ANIMATION_NORMALIZED_MAX because the right flip animation is actually
  // played backwards
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX,
                                                          CompositorTransitionDirectionRight,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__right_flip_first_frame_clipping(void) {
  prv_test_clipping_mask(prv_right_flip_first_frame_clipping, "right_flip_first_frame_clipping");
};

// This test records a clipping mask of the 1/4 progress frame of the right "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_right_flip_first_quarter_frame_clipping(GContext *ctx) {
  // The 1/4 progress frame is ANIMATION_NORMALIZED_MAX * 3/4 because the right flip animation is
  // actually played backwards
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX * 3 / 4,
                                                          CompositorTransitionDirectionRight,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__right_flip_first_quarter_frame_clipping(void) {
  prv_test_clipping_mask(prv_right_flip_first_quarter_frame_clipping,
                         "right_flip_first_quarter_frame_clipping");
};

// This test records a clipping mask of the half progress frame of the right "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_right_flip_half_frame_clipping(GContext *ctx) {
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX / 2,
                                                          CompositorTransitionDirectionRight,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__right_flip_half_frame_clipping(void) {
  prv_test_clipping_mask(prv_right_flip_half_frame_clipping, "right_flip_half_frame_clipping");
};

// This test records a clipping mask of the 3/4 progress frame of the right "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_right_flip_third_quarter_frame_clipping(GContext *ctx) {
  // The 3/4 progress frame is ANIMATION_NORMALIZED_MAX * 1/4 because the right flip animation is
  // actually played backwards
  compositor_round_flip_transitions_flip_animation_update(ctx, ANIMATION_NORMALIZED_MAX / 4,
                                                          CompositorTransitionDirectionRight,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__right_flip_third_quarter_frame_clipping(void) {
  prv_test_clipping_mask(prv_right_flip_third_quarter_frame_clipping,
                         "right_flip_third_quarter_frame_clipping");
};

// This test records a clipping mask of the last frame of the right "round flip" compositor
// transition animation and then clips a full-screen red rectangle to the resulting mask

static void prv_right_flip_last_frame_clipping(GContext *ctx) {
  // The last frame is 0 because the right flip animation is actually played backwards
  compositor_round_flip_transitions_flip_animation_update(ctx, 0,
                                                          CompositorTransitionDirectionRight,
                                                          GColorWhite);
}

void test_graphics_window_stack_animation__right_flip_last_frame_clipping(void) {
  prv_test_clipping_mask(prv_right_flip_last_frame_clipping, "right_flip_last_frame_clipping");
};

void test_graphics_window_stack_animation__move_pixels_horizontally(void) {
  GContext *ctx = malloc(sizeof(GContext));
  test_graphics_context_init(ctx, fb);
  framebuffer_clear(fb);

  GBitmap *const bitmap = &ctx->dest_bitmap;

  // vertical test pattern
  for (int16_t y = 0; y < DISP_ROWS; y++) {
    GBitmapDataRowInfo row_info = prv_gbitmap_get_data_row_info(bitmap, y);
    for (int x = 0; x < DISP_COLS; x++) {
      GColor8 color = GColorFromRGB(1 * x * UINT8_MAX / DISP_COLS,
                                    2 * x * UINT8_MAX / DISP_COLS,
                                    4 * x * UINT8_MAX / DISP_COLS);
      if (row_info.min_x <= x && x <= row_info.max_x) {
        row_info.data[x] = color.argb;
      }
    }
  }


  // nop
  graphics_private_move_pixels_horizontally(NULL, 50, false);
  graphics_private_move_pixels_horizontally(bitmap, 0, false);

  graphics_private_move_pixels_horizontally(bitmap, 50, false);
  cl_check(gbitmap_pbi_eq(bitmap, TEST_NAMED_PBI_FILE("move_horizontal_right")));

  graphics_private_move_pixels_horizontally(bitmap, -100, false);
  cl_check(gbitmap_pbi_eq(bitmap, TEST_NAMED_PBI_FILE("move_horizontal_left")));

  graphics_private_move_pixels_horizontally(bitmap, 400, false);
  cl_check(gbitmap_pbi_eq(bitmap, TEST_NAMED_PBI_FILE("move_horizontal_right_too_far")));

  graphics_private_move_pixels_horizontally(bitmap, -400, true);
  cl_check(gbitmap_pbi_eq(bitmap, TEST_NAMED_PBI_FILE("move_horizontal_left_filled")));

  free(ctx);
}
