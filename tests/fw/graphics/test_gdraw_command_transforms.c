/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gdraw_command_private.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_line.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/framebuffer.h"
#include "applib/ui/animation.h"
#include "applib/ui/animation_timing.h"
#include "applib/ui/animation_interpolate.h"

#include "applib/graphics/gdraw_command_transforms.h"

#include "util.h"
#include "test_graphics.h"
#include "8bit/test_framebuffer.h"
#include "weather_app_resources.h"

// Stubs
////////////////////////////////////
#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"

#include <stdio.h>

// stubs

InterpolateInt64Function animation_private_current_interpolate_override(void) {
  return NULL;
}

static FrameBuffer *fb = NULL;

// Setup
void test_gdraw_command_transforms__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_gdraw_command_transforms__cleanup(void) {
  free(fb);
}

void test_gdraw_command_transforms__to_square(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  graphics_context_set_fill_color(&ctx, GColorFromHEX(0x55aaff));
  graphics_context_set_antialiased(&ctx, true);
  graphics_fill_rect(&ctx, &ctx.dest_bitmap.bounds);

  GDrawCommandImage *l = weather_app_resource_create_sun();
  gdraw_command_image_draw(&ctx, l, GPoint(0, 0));

  int32_t dt = ANIMATION_NORMALIZED_MAX / 5;
  int32_t t = 0;

  l = weather_app_resource_create_sun();
  gdraw_command_image_attract_to_square(l, t += dt);
  gdraw_command_image_draw(&ctx, l, GPoint(48, 0));

  l = weather_app_resource_create_sun();
  gdraw_command_image_attract_to_square(l, t += dt);
  gdraw_command_image_draw(&ctx, l, GPoint(0, 48));

  l = weather_app_resource_create_sun();
  gdraw_command_image_attract_to_square(l, t += dt);
  gdraw_command_image_draw(&ctx, l, GPoint(48, 48));

  l = weather_app_resource_create_sun();
  gdraw_command_image_attract_to_square(l, t += dt);
  gdraw_command_image_draw(&ctx, l, GPoint(0, 96));

  l = weather_app_resource_create_sun();
  gdraw_command_image_attract_to_square(l, t += dt);
  gdraw_command_image_draw(&ctx, l, GPoint(48, 96));

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "test_gdraw_command_transforms__to_square.8bit.pbi"));
}

// re-enable this "test" to debug per-frame transitions
void _test_gdraw_command_transforms__to_square_sequence(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  graphics_context_set_antialiased(&ctx, true);

  int32_t dt = ANIMATION_NORMALIZED_MAX / 16;
  int32_t t = 0;

  while (t <= ANIMATION_NORMALIZED_MAX) {
    graphics_context_set_fill_color(&ctx, GColorFromHEX(0x55aaff));
    graphics_fill_rect(&ctx, &ctx.dest_bitmap.bounds);
    GDrawCommandImage *img = weather_app_resource_create_sun();
    gdraw_command_image_attract_to_square(img, t);
    gdraw_command_image_draw(&ctx, img, GPoint(48, 96));
    char fn[20];
    snprintf(fn, sizeof(fn), "tos_%06d.png", t);
    tests_write_gbitmap_to_pbi(&ctx.dest_bitmap, fn);
    free(img);
    t += dt;
  }
}

int16_t prv_int_scale_and_translate_to(
    int16_t value, int16_t size, int16_t from_range, int16_t to_range,
    int16_t from_min, int16_t to_min, int32_t normalized, InterpolateInt64Function interpolate);

int64_t prv_default_interpolate(int32_t normalized, int64_t from, int64_t to);

void test_gdraw_command_transforms__int_scale_to_translate_overflow(void) {
  InterpolateInt64Function interp = prv_default_interpolate;
  int y = prv_int_scale_and_translate_to(255, 10, 10, 10, 0, 255, ANIMATION_NORMALIZED_MAX,
                                         interp);
  cl_assert_equal_i(y, 255 + 255);
}

void test_gdraw_command_transforms__int_scale_to_translate_overflow_neg(void) {
  InterpolateInt64Function interp = prv_default_interpolate;
  int y = prv_int_scale_and_translate_to(-255, 10, 10, 10, 0, -255, ANIMATION_NORMALIZED_MAX,
                                         interp);
  cl_assert_equal_i(y, -255 + -255);
}

void test_gdraw_command_transforms__int_scale_to_scale_overflow(void) {
  InterpolateInt64Function interp = prv_default_interpolate;
  int y = prv_int_scale_and_translate_to(181, 1, 1, 181, 0, 0, ANIMATION_NORMALIZED_MAX,
                                         interp);
  cl_assert_equal_i(y, 181 * 181);
}

void test_gdraw_command_transforms__int_scale_to_scale_overflow_neg(void) {
  InterpolateInt64Function interp = prv_default_interpolate;
  int y = prv_int_scale_and_translate_to(-181, 1, 1, 181, 0, 0, ANIMATION_NORMALIZED_MAX,
                                         interp);
  cl_assert_equal_i(y, -181 * 181);
}

void test_gdraw_command_transforms__segmented_scale(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);
  graphics_context_set_fill_color(&ctx, GColorFromHEX(0x55aaff));
  graphics_context_set_antialiased(&ctx, true);
  graphics_fill_rect(&ctx, &ctx.dest_bitmap.bounds);

  GDrawCommandImage *l = weather_app_resource_create_sun();
  gdraw_command_image_draw(&ctx, l, GPointZero);

  int32_t dt = ANIMATION_NORMALIZED_MAX / 5;
  int32_t t = 0;
  InterpolateInt64Function interp = NULL;

  int16_t s = 48;
  GRect from = GRect(0, 0, s, s);
  GRect to = GRect(90, 0, s, s);

  GPointIndexLookup *index_lookup = gdraw_command_list_create_index_lookup_by_distance(
     gdraw_command_image_get_command_list(l), GPoint(s, s / 2));

  Fixed_S32_16 f = Fixed_S32_16(FIXED_S32_16_ONE.raw_value / 2);

  l = weather_app_resource_create_sun();
  gdraw_command_image_scale_segmented_to(l, from, to, t += dt, interp, index_lookup, f, false);
  gdraw_command_image_draw(&ctx, l, GPoint(0, 0));

  l = weather_app_resource_create_sun();
  gdraw_command_image_scale_segmented_to(l, from, to, t += dt, interp, index_lookup, f, false);
  gdraw_command_image_draw(&ctx, l, GPoint(0, 48));

  l = weather_app_resource_create_sun();
  gdraw_command_image_scale_segmented_to(l, from, to, t += dt, interp, index_lookup, f, false);
  gdraw_command_image_draw(&ctx, l, GPoint(0, 96));

  l = weather_app_resource_create_sun();
  gdraw_command_image_scale_segmented_to(l, from, to, t += dt, interp, index_lookup, f, false);
  gdraw_command_image_draw(&ctx, l, GPoint(0, 144));

  l = weather_app_resource_create_sun();
  gdraw_command_image_scale_segmented_to(l, from, to, t += dt, interp, index_lookup, f, false);
  gdraw_command_image_draw(&ctx, l, GPoint(0, 192));

  free(index_lookup);

  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "test_gdraw_command_transforms__segmented_scale.8bit.pbi"));
}

// re-enable this "test" to debug per-frame transitions
void _test_gdraw_command_transforms__scale_segmented_sequence(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  graphics_context_set_antialiased(&ctx, true);

  int32_t dt = ANIMATION_NORMALIZED_MAX / 16;
  int32_t t = 0;
  InterpolateInt64Function interp = NULL;

  int16_t s = 48;
  GRect from = GRect(144 - s * 3 / 4, s / 4, s / 2, s / 2);
  GRect to = GRect(s / 2, s, 2 * s, 2 * s);

  GDrawCommandImage *img = weather_app_resource_create_cloud();
  GPointIndexLookup *index_lookup = gdraw_command_list_create_index_lookup_by_distance(
     gdraw_command_image_get_command_list(img), GPoint(s / 2, s));
  free(img);

  Fixed_S32_16 f = Fixed_S32_16(FIXED_S32_16_ONE.raw_value / 8);

  while (t <= ANIMATION_NORMALIZED_MAX) {
    graphics_context_set_fill_color(&ctx, GColorFromHEX(0x55aaff));
    graphics_fill_rect(&ctx, &ctx.dest_bitmap.bounds);
    img = weather_app_resource_create_cloud();
    gdraw_command_image_scale_segmented_to(img, from, to, t, interp, index_lookup, f, false);
    gdraw_command_image_draw(&ctx, img, GPointZero);
    char fn[20];
    snprintf(fn, sizeof(fn), "elo_%06d.png", t);
    tests_write_gbitmap_to_pbi(&ctx.dest_bitmap, fn);
    t += dt;
  }
}

