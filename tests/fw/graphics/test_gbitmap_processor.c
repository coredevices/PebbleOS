/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/framebuffer.h"

// Stubs
///////////////////////

#include "stubs_app_state.h"
#include "stubs_graphics_circle.h"
#include "stubs_graphics_line.h"
#include "stubs_graphics_private.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_process_manager.h"

const GDrawRawImplementation g_default_draw_implementation;

// Statics
///////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

typedef struct MockBitbltBitmapIntoBitmapTiledCallRecording {
  GBitmap *dest_bitmap;
  const GBitmap *src_bitmap;
  GRect dest_rect;
  GPoint src_origin_offset;
  GCompOp compositing_mode;
  GColor8 tint_color;
} MockBitbltBitmapIntoBitmapTiledCallRecording;

typedef struct MockBitbltBitmapIntoBitmapTiledCallRecordings {
  unsigned int call_count;
  MockBitbltBitmapIntoBitmapTiledCallRecording last_call;
} MockBitbltBitmapIntoBitmapTiledCallRecordings;

static MockBitbltBitmapIntoBitmapTiledCallRecordings s_bitblt_bitmap_into_bitmap_tiled_calls;

// Fakes
///////////////////////

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void bitblt_bitmap_into_bitmap_tiled(GBitmap* dest_bitmap, const GBitmap *src_bitmap,
                                     GRect dest_rect, GPoint src_origin_offset,
                                     GCompOp compositing_mode, GColor8 tint_color) {
  s_bitblt_bitmap_into_bitmap_tiled_calls.call_count++;
  s_bitblt_bitmap_into_bitmap_tiled_calls.last_call =
    (MockBitbltBitmapIntoBitmapTiledCallRecording) {
    .dest_bitmap = dest_bitmap,
    .src_bitmap = src_bitmap,
    .dest_rect = dest_rect,
    .src_origin_offset = src_origin_offset,
    .compositing_mode = compositing_mode,
    .tint_color = tint_color,
  };
}

// Helpers
///////////////////////

static void cl_assert_equal_rect(const GRect a, const GRect b) {
  cl_assert_equal_i(a.origin.x, b.origin.x);
  cl_assert_equal_i(a.origin.y, b.origin.y);
  cl_assert_equal_i(a.size.w, b.size.w);
  cl_assert_equal_i(a.size.h, b.size.h);
}

// Setup
///////////////////////

void test_gbitmap_processor__initialize(void) {
  s_fb = (FrameBuffer) {};
  framebuffer_init(&s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_bitblt_bitmap_into_bitmap_tiled_calls = (MockBitbltBitmapIntoBitmapTiledCallRecordings) {};
}

void test_gbitmap_processor__cleanup(void) {
}

// Tests
///////////////////////

#define EXPECTED_RECT_IN_PRE_FUNCTION (GRect(4, 3, 2, 1))

void test_gbitmap_processor__null_arguments(void) {
  GBitmap bitmap = {0};
  const GRect rect = EXPECTED_RECT_IN_PRE_FUNCTION;

  // Passing NULL for the processor shouldn't cause any problems
  graphics_draw_bitmap_in_rect_processed(&s_ctx, &bitmap, &rect, NULL);
  // And it should try to draw the bitmap
  cl_assert_equal_i(s_bitblt_bitmap_into_bitmap_tiled_calls.call_count, 1);

  // Passing a processor with NULL functions shouldn't cause any problems
  GBitmapProcessor processor = {0};
  graphics_draw_bitmap_in_rect_processed(&s_ctx, &bitmap, &rect, &processor);
  // And it should once again try to draw the bitmap
  cl_assert_equal_i(s_bitblt_bitmap_into_bitmap_tiled_calls.call_count, 2);
}

#define EXPECTED_COMPOSITING_MODE_BEFORE_AND_AFTER_PRE_FUNCTION (GCompOpSet)
#define EXPECTED_TINT_COLOR_BEFORE_AND_AFTER_PRE_FUNCTION (GColorShockingPink)

#define COMPOSITING_MODE_TO_SPECIFY_IN_PRE_FUNCTION (GCompOpTint)
#define TINT_COLOR_TO_SPECIFY_IN_PRE_FUNCTION (GColorTiffanyBlue)
#define RECT_TO_SPECIFY_IN_PRE_FUNCTION (GRect(-50, -50, 100, 100))
#define BITMAP_TO_SPECIFY_IN_PRE_FUNCTION ((GBitmap *)1234)

#define EXPECTED_CLIPPED_RECT_AFTER_DRAWING_BITMAP (GRect(0, 0, 50, 50))

typedef struct PreAndPostFunctionsTestProcessor {
  GBitmapProcessor processor;
  GCompOp previous_compositing_mode;
  GColor previous_tint_color;
} PreAndPostFunctionsTestProcessor;

static void prv_pre_and_post_functions__pre(GBitmapProcessor *processor, GContext *ctx,
                                            const GBitmap **bitmap_to_use,
                                            GRect *global_grect_to_use) {
  PreAndPostFunctionsTestProcessor *processor_with_data =
    (PreAndPostFunctionsTestProcessor *)processor;

  // Record the existing compositing mode and tint color and check that they are what we expect
  cl_assert(ctx->draw_state.compositing_mode ==
              EXPECTED_COMPOSITING_MODE_BEFORE_AND_AFTER_PRE_FUNCTION);
  processor_with_data->previous_compositing_mode = ctx->draw_state.compositing_mode;
  cl_assert(gcolor_equal(ctx->draw_state.tint_color,
                         EXPECTED_TINT_COLOR_BEFORE_AND_AFTER_PRE_FUNCTION));
  processor_with_data->previous_tint_color = ctx->draw_state.tint_color;

  // Set the compositing mode and tint color to different values
  ctx->draw_state.compositing_mode = COMPOSITING_MODE_TO_SPECIFY_IN_PRE_FUNCTION;
  ctx->draw_state.tint_color = TINT_COLOR_TO_SPECIFY_IN_PRE_FUNCTION;

  // Check that the rect here is what we gave to graphics_draw_bitmap_in_rect_processed()
  cl_assert_equal_rect(*global_grect_to_use, EXPECTED_RECT_IN_PRE_FUNCTION);

  // Change the rect
  *global_grect_to_use = RECT_TO_SPECIFY_IN_PRE_FUNCTION;

  // Change the bitmap
  *bitmap_to_use = BITMAP_TO_SPECIFY_IN_PRE_FUNCTION;
}

static void prv_pre_and_post_functions__post(GBitmapProcessor *processor, GContext *ctx,
                                             const GBitmap *bitmap_used,
                                             const GRect *global_clipped_grect_used) {
  PreAndPostFunctionsTestProcessor *processor_with_data =
    (PreAndPostFunctionsTestProcessor *)processor;

  // Check that the changes made to the GContext in .pre are still present
  cl_assert(ctx->draw_state.compositing_mode == COMPOSITING_MODE_TO_SPECIFY_IN_PRE_FUNCTION);
  cl_assert(gcolor_equal(ctx->draw_state.tint_color, TINT_COLOR_TO_SPECIFY_IN_PRE_FUNCTION));

  // Reverse the changes to the GContext that were made in .pre
  ctx->draw_state.compositing_mode = processor_with_data->previous_compositing_mode;
  ctx->draw_state.tint_color = processor_with_data->previous_tint_color;

  // Check that the bitmap here is the bitmap we specified in the .pre function
  cl_assert_equal_p(bitmap_used, BITMAP_TO_SPECIFY_IN_PRE_FUNCTION);

  // Check that the rect here is the clipped version of the rect we specified in the .pre function
  cl_assert_equal_rect(*global_clipped_grect_used, EXPECTED_CLIPPED_RECT_AFTER_DRAWING_BITMAP);
}

void test_gbitmap_processor__pre_and_post_functions(void) {
  GBitmap bitmap = {0};
  const GRect rect = EXPECTED_RECT_IN_PRE_FUNCTION;

  // Set the compositing mode and tint color to known values
  s_ctx.draw_state.compositing_mode = EXPECTED_COMPOSITING_MODE_BEFORE_AND_AFTER_PRE_FUNCTION;
  s_ctx.draw_state.tint_color = EXPECTED_TINT_COLOR_BEFORE_AND_AFTER_PRE_FUNCTION;

  PreAndPostFunctionsTestProcessor processor = (PreAndPostFunctionsTestProcessor) {
    .processor.pre = prv_pre_and_post_functions__pre,
    .processor.post = prv_pre_and_post_functions__post,
  };
  graphics_draw_bitmap_in_rect_processed(&s_ctx, &bitmap, &rect, &processor.processor);

  // Check that the bitmap was drawn
  cl_assert_equal_i(s_bitblt_bitmap_into_bitmap_tiled_calls.call_count, 1);

  // Check that the modifications made in the .pre function propagated to the bitmap drawing
  cl_assert(s_bitblt_bitmap_into_bitmap_tiled_calls.last_call.compositing_mode ==
              COMPOSITING_MODE_TO_SPECIFY_IN_PRE_FUNCTION);
  cl_assert(gcolor_equal(s_bitblt_bitmap_into_bitmap_tiled_calls.last_call.tint_color,
                         TINT_COLOR_TO_SPECIFY_IN_PRE_FUNCTION));
  cl_assert_equal_rect(s_bitblt_bitmap_into_bitmap_tiled_calls.last_call.dest_rect,
                       EXPECTED_CLIPPED_RECT_AFTER_DRAWING_BITMAP);
  cl_assert_equal_p(s_bitblt_bitmap_into_bitmap_tiled_calls.last_call.src_bitmap,
                    BITMAP_TO_SPECIFY_IN_PRE_FUNCTION);

  // Check that the modifications made to the GContext in the .pre function were reversed in .post
  cl_assert(s_ctx.draw_state.compositing_mode ==
              EXPECTED_COMPOSITING_MODE_BEFORE_AND_AFTER_PRE_FUNCTION);
  cl_assert(gcolor_equal(s_ctx.draw_state.tint_color,
                         EXPECTED_TINT_COLOR_BEFORE_AND_AFTER_PRE_FUNCTION));

  // Note that additional checks are performed in the .pre and .post functions
}

typedef struct PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor {
  GBitmapProcessor processor;
  const GBitmap *expected_bitmap_in_post;
  bool post_func_called;
} PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor;

static void post_function_called_even_if_pre_function_causes_nothing_to_be_drawn__post(
  GBitmapProcessor *processor, GContext *ctx, const GBitmap *bitmap_used,
  const GRect *global_clipped_grect_used) {
  PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor *processor_with_data =
    (PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor *)processor;

  // Check that the rectangle here is empty to verify that nothing was drawn
  cl_assert_equal_rect(*global_clipped_grect_used, GRectZero);

  // Check that bitmap_used is what we expect it to be (the expected value is set in .pre)
  cl_assert_equal_p(bitmap_used, processor_with_data->expected_bitmap_in_post);

  // Record that the .post function was called
  processor_with_data->post_func_called = true;
}

//! Helper function for testing that the .post function is called even if the .pre function
//! causes no bitmap to be drawn
static void prv_post_function_called_even_if_pre_function_causes_nothing_to_be_drawn_test(
  GBitmapProcessorPreFunc pre_func) {
  GBitmap bitmap = {0};
  const GRect rect = EXPECTED_RECT_IN_PRE_FUNCTION;

  PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor processor =
    (PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor) {
      .processor.pre = pre_func,
      .processor.post = post_function_called_even_if_pre_function_causes_nothing_to_be_drawn__post,
    };
  graphics_draw_bitmap_in_rect_processed(&s_ctx, &bitmap, &rect, &processor.processor);

  // Check that the bitmap was not drawn
  cl_assert_equal_i(s_bitblt_bitmap_into_bitmap_tiled_calls.call_count, 0);

  // Check that the .post function was called even though the .pre function makes some change
  // that causes no bitmap to be drawn
  cl_assert(processor.post_func_called);
}

static void post_function_called_even_if_pre_function_specifies_null_bitmap__pre(
  GBitmapProcessor *processor, GContext *ctx, const GBitmap **bitmap_to_use,
  GRect *global_grect_to_use) {
  PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor *processor_with_data =
    (PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor *)processor;

  // Change the bitmap to use to NULL to cause nothing to be drawn
  *bitmap_to_use = NULL;

  // We'll expect the bitmap we set there to be the bitmap passed into .post
  processor_with_data->expected_bitmap_in_post = *bitmap_to_use;
}

void test_gbitmap_processor__post_function_called_even_if_pre_function_specifies_null_bitmap(void) {
  prv_post_function_called_even_if_pre_function_causes_nothing_to_be_drawn_test(
    post_function_called_even_if_pre_function_specifies_null_bitmap__pre);
};

static void post_function_called_even_if_pre_function_specifies_empty_rect__pre(
  GBitmapProcessor *processor, GContext *ctx, const GBitmap **bitmap_to_use,
  GRect *global_grect_to_use) {
  PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor *processor_with_data =
    (PostFunctionCalledEvenIfPreFunctionCausesNothingToBeDrawnTestProcessor *)processor;

  // Change the rectangle to be empty to cause nothing to be drawn
  *global_grect_to_use = GRectZero;

  // We'll expect the bitmap we passed into graphics_draw_bitmap_in_rect_processed() in .post
  // even though nothing will be drawn
  processor_with_data->expected_bitmap_in_post = *bitmap_to_use;
}

void test_gbitmap_processor__post_function_called_even_if_pre_function_specifies_empty_rect(void) {
  prv_post_function_called_even_if_pre_function_causes_nothing_to_be_drawn_test(
    post_function_called_even_if_pre_function_specifies_empty_rect__pre);
};
