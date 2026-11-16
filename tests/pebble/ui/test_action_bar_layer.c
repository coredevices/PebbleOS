/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <time.h>

#include <applib/graphics/gtypes.h>
#include <applib/ui/action_bar_layer.h>
#include <applib/ui/action_bar_layer_private.h>
#include <applib/ui/animation.h>
#include <applib/ui/animation_timing.h>
#include <applib/ui/window.h>
#include <clar.h>
#include <kernel/pebble_tasks.h>
#include <process_management/pebble_process_md.h>
#include <stubs_app_state.h>
#include <stubs_compiled_with_legacy2_sdk.h>
#include <stubs_logging.h>
#include <stubs_pbl_malloc.h>

static bool s_left_handed;
static PebbleTask s_task = PebbleTask_App;
static ProcessAppSDKType s_sdk_type = ProcessAppSDKType_System;
static GRect s_drawn_icon_rect;

bool display_orientation_is_left(void) {
  cl_assert_(false, "applib must read orientation through a syscall");
  return false;
}

bool sys_display_orientation_is_left(void) {
  return s_left_handed;
}

PebbleTask pebble_task_get_current(void) {
  return s_task;
}

const PebbleProcessMd *sys_process_manager_get_current_process_md(void) {
  cl_assert_(false, "applib must not dereference process metadata");
  return nullptr;
}

ProcessAppSDKType process_metadata_get_app_sdk_type(const PebbleProcessMd *md) {
  cl_assert_(false, "applib must not dereference process metadata");
  return ProcessAppSDKType_System;
}

ProcessAppSDKType sys_process_manager_get_current_process_sdk_type(void) {
  return s_sdk_type;
}

PlatformType process_manager_current_platform(void) {
  return PBL_PLATFORM_TYPE_CURRENT;
}

void window_schedule_render(struct Window *window) {
  (void)window;
}

void recognizer_destroy(Recognizer *recognizer) {
  (void)recognizer;
}

void recognizer_add_to_list(Recognizer *recognizer, RecognizerList *list) {
  (void)recognizer;
  (void)list;
}

void recognizer_remove_from_list(Recognizer *recognizer, RecognizerList *list) {
  (void)recognizer;
  (void)list;
}

RecognizerManager *window_get_recognizer_manager(Window *window) {
  (void)window;
  return nullptr;
}

void recognizer_manager_cancel_and_reset(RecognizerManager *manager) {
  (void)manager;
}

bool recognizer_list_iterate(RecognizerList *list, RecognizerListIteratorCb iter_cb,
                             void *context) {
  (void)list;
  (void)iter_cb;
  (void)context;
  return false;
}

void recognizer_manager_register_recognizer(RecognizerManager *manager, Recognizer *recognizer) {
  (void)manager;
  (void)recognizer;
}

void recognizer_manager_deregister_recognizer(RecognizerManager *manager, Recognizer *recognizer) {
  (void)manager;
  (void)recognizer;
}

GDrawState graphics_context_get_drawing_state(GContext *ctx) {
  (void)ctx;
  return (GDrawState){0};
}

void graphics_context_set_drawing_state(GContext *ctx, GDrawState draw_state) {
  (void)ctx;
  (void)draw_state;
}

bool graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) {
  (void)ctx;
  (void)buffer;
  return false;
}

AnimationProgress animation_timing_curve(AnimationProgress time_normalized, AnimationCurve curve) {
  (void)curve;
  return time_normalized;
}

void window_set_click_config_provider_with_context(Window *window,
                                                   ClickConfigProvider click_config_provider,
                                                   void *context) {
  (void)window;
  (void)click_config_provider;
  (void)context;
}

void window_raw_click_subscribe(ButtonId button_id, ClickHandler down_handler,
                                ClickHandler up_handler, void *context) {
  (void)button_id;
  (void)down_handler;
  (void)up_handler;
  (void)context;
}

void window_set_click_context(ButtonId button_id, void *context) {
  (void)button_id;
  (void)context;
}

void graphics_context_set_fill_color(GContext *ctx, GColor color) {
  (void)ctx;
  (void)color;
}

void graphics_context_set_compositing_mode(GContext *ctx, GCompOp mode) {
  (void)ctx;
  (void)mode;
}

void graphics_fill_rect(GContext *ctx, const GRect *rect) {
  (void)ctx;
  (void)rect;
}

void graphics_fill_oval(GContext *ctx, GRect rect, GOvalScaleMode scale_mode) {
  (void)ctx;
  (void)rect;
  (void)scale_mode;
}

void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, const GRect *rect) {
  (void)ctx;
  (void)bitmap;
  s_drawn_icon_rect = *rect;
}

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  (void)tloc;
  if (out_ms) {
    *out_ms = 0;
  }
  return 0;
}

static void prv_reset(void) {
  s_left_handed = false;
  s_task = PebbleTask_App;
  s_sdk_type = ProcessAppSDKType_System;
}

void test_action_bar_layer__initialize(void) {
  prv_reset();
}

void test_action_bar_layer__cleanup(void) {
  prv_reset();
}

static int16_t prv_add_and_get_x(void) {
  Window window = {};
  layer_init(&window.layer, &GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  ActionBarLayer bar;
  action_bar_layer_init(&bar);
  action_bar_layer_add_to_window(&bar, &window);
  const int16_t x = bar.layer.frame.origin.x;
  action_bar_layer_deinit(&bar);
  layer_deinit(&window.layer);
  return x;
}

void test_action_bar_layer__system_app_stays_right_when_not_left_handed(void) {
  cl_assert(action_bar_layer_is_on_right());
  cl_assert_equal_i(prv_add_and_get_x(), PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
  cl_assert_equal_i(action_bar_layer_get_content_origin_x(), 0);
}

void test_action_bar_layer__system_app_moves_left_when_left_handed(void) {
  s_left_handed = true;
  cl_assert(!action_bar_layer_is_on_right());
  cl_assert_equal_i(prv_add_and_get_x(), 0);
  cl_assert_equal_i(action_bar_layer_get_content_origin_x(), ACTION_BAR_WIDTH);
}

void test_action_bar_layer__third_party_app_stays_right_when_left_handed(void) {
  s_left_handed = true;
  s_sdk_type = ProcessAppSDKType_4x;
  cl_assert(action_bar_layer_is_on_right());
  cl_assert_equal_i(prv_add_and_get_x(), PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
}

void test_action_bar_layer__kernel_ui_moves_left_when_left_handed(void) {
  s_left_handed = true;
  s_task = PebbleTask_KernelMain;
  s_sdk_type = ProcessAppSDKType_4x;
  cl_assert(!action_bar_layer_is_on_right());
  cl_assert_equal_i(prv_add_and_get_x(), 0);
}

void test_action_bar_layer__inset_bounds_shifts_origin_when_on_left(void) {
  s_left_handed = true;
  const GRect inset =
      action_bar_layer_preferred_inset_bounds(GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  cl_assert_equal_i(inset.origin.x, ACTION_BAR_WIDTH);
  cl_assert_equal_i(inset.size.w, PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
}

void test_action_bar_layer__attached_bar_keeps_drawing_side_after_pref_change(void) {
  s_left_handed = true;
  Window window = {};
  layer_init(&window.layer, &GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  ActionBarLayer bar;
  action_bar_layer_init(&bar);
  action_bar_layer_add_to_window(&bar, &window);
  GBitmap icon = {.bounds = GRect(0, 0, 10, 10)};
  bar.icons[0] = &icon;
  bar.is_highlighted = 1;
  bar.animation[0] = ActionBarLayerIconPressAnimationMoveLeft;
  GContext ctx = {};
  bar.layer.update_proc(&bar.layer, &ctx);
  const int16_t before = s_drawn_icon_rect.origin.x;

  s_left_handed = false;
  GRect sliding_frame = bar.layer.frame;
  sliding_frame.origin.x = -1;
  layer_set_frame(&bar.layer, &sliding_frame);
  bar.layer.update_proc(&bar.layer, &ctx);
  cl_assert_equal_i(s_drawn_icon_rect.origin.x, before);

  action_bar_layer_add_to_window(&bar, &window);
  cl_assert_equal_i(bar.layer.frame.origin.x, PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
  bar.layer.update_proc(&bar.layer, &ctx);
  cl_assert(s_drawn_icon_rect.origin.x < before);
  action_bar_layer_deinit(&bar);
  layer_deinit(&window.layer);
}

typedef struct {
  Window window;
  ActionBarLayer bar;
} BarFixture;

static void prv_fixture_init(BarFixture *f, GRect window_bounds) {
  *f = (BarFixture){};
  layer_init(&f->window.layer, &window_bounds);
  action_bar_layer_init(&f->bar);
}

static void prv_fixture_deinit(BarFixture *f) {
  action_bar_layer_deinit(&f->bar);
  layer_deinit(&f->window.layer);
}

void test_action_bar_layer__third_party_bar_moves_left_when_opted_in(void) {
  s_left_handed = true;
  s_sdk_type = ProcessAppSDKType_4x;
  BarFixture f;
  prv_fixture_init(&f, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_set_follows_orientation(&f.bar, true);
  cl_assert(action_bar_layer_is_on_left(&f.bar));
  action_bar_layer_add_to_window(&f.bar, &f.window);
  cl_assert_equal_i(f.bar.layer.frame.origin.x, 0);
  cl_assert(action_bar_layer_is_on_left(&f.bar));
  prv_fixture_deinit(&f);
}

void test_action_bar_layer__third_party_opt_in_stays_right_when_not_left_handed(void) {
  s_sdk_type = ProcessAppSDKType_4x;
  BarFixture f;
  prv_fixture_init(&f, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_set_follows_orientation(&f.bar, true);
  action_bar_layer_add_to_window(&f.bar, &f.window);
  cl_assert(!action_bar_layer_is_on_left(&f.bar));
  cl_assert_equal_i(f.bar.layer.frame.origin.x, PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
  prv_fixture_deinit(&f);
}

void test_action_bar_layer__third_party_bar_stays_right_without_opt_in(void) {
  s_left_handed = true;
  s_sdk_type = ProcessAppSDKType_4x;
  BarFixture f;
  prv_fixture_init(&f, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_add_to_window(&f.bar, &f.window);
  cl_assert(!action_bar_layer_is_on_left(&f.bar));
  const GRect inset =
      action_bar_layer_inset_bounds(&f.bar, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  cl_assert_equal_i(inset.origin.x, 0);
  cl_assert_equal_i(inset.size.w, PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
  prv_fixture_deinit(&f);
}

void test_action_bar_layer__opt_in_is_per_bar(void) {
  s_left_handed = true;
  s_sdk_type = ProcessAppSDKType_4x;
  BarFixture a;
  BarFixture b;
  prv_fixture_init(&a, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  prv_fixture_init(&b, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_set_follows_orientation(&a.bar, true);
  action_bar_layer_add_to_window(&a.bar, &a.window);
  action_bar_layer_add_to_window(&b.bar, &b.window);
  cl_assert(action_bar_layer_is_on_left(&a.bar));
  cl_assert(!action_bar_layer_is_on_left(&b.bar));
  // Third-party bar-less queries keep the right-edge default.
  cl_assert(action_bar_layer_is_on_right());
  prv_fixture_deinit(&a);
  prv_fixture_deinit(&b);
}

void test_action_bar_layer__toggling_attached_bar_moves_it(void) {
  s_left_handed = true;
  s_sdk_type = ProcessAppSDKType_4x;
  BarFixture f;
  prv_fixture_init(&f, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_add_to_window(&f.bar, &f.window);
  cl_assert_equal_i(f.bar.layer.frame.origin.x, PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);

  GBitmap icon = {.bounds = GRect(0, 0, 10, 10)};
  f.bar.icons[0] = &icon;
  f.bar.is_highlighted = 1;
  f.bar.animation[0] = ActionBarLayerIconPressAnimationMoveLeft;
  GContext ctx = {};
  f.bar.layer.update_proc(&f.bar.layer, &ctx);
  const int16_t icon_x_right = s_drawn_icon_rect.origin.x;

  action_bar_layer_set_follows_orientation(&f.bar, true);
  cl_assert_equal_i(f.bar.layer.frame.origin.x, 0);
  cl_assert(action_bar_layer_is_on_left(&f.bar));
  // MoveLeft now presses toward the content, which is to the right.
  f.bar.layer.update_proc(&f.bar.layer, &ctx);
  cl_assert(s_drawn_icon_rect.origin.x > icon_x_right);

  action_bar_layer_set_follows_orientation(&f.bar, false);
  cl_assert_equal_i(f.bar.layer.frame.origin.x, PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
  cl_assert(!action_bar_layer_is_on_left(&f.bar));
  prv_fixture_deinit(&f);
}

void test_action_bar_layer__attached_bar_keeps_side_after_pref_change(void) {
  s_left_handed = true;
  s_sdk_type = ProcessAppSDKType_4x;
  BarFixture f;
  prv_fixture_init(&f, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_set_follows_orientation(&f.bar, true);
  action_bar_layer_add_to_window(&f.bar, &f.window);
  s_left_handed = false;
  cl_assert(action_bar_layer_is_on_left(&f.bar));
  cl_assert_equal_i(f.bar.layer.frame.origin.x, 0);
  prv_fixture_deinit(&f);
}

void test_action_bar_layer__system_bar_follows_orientation_without_opt_in(void) {
  s_left_handed = true;
  BarFixture f;
  prv_fixture_init(&f, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_set_follows_orientation(&f.bar, false);
  cl_assert(action_bar_layer_is_on_left(&f.bar));
  action_bar_layer_add_to_window(&f.bar, &f.window);
  cl_assert_equal_i(f.bar.layer.frame.origin.x, 0);
  prv_fixture_deinit(&f);
}

void test_action_bar_layer__inset_bounds_follows_bar_side(void) {
  s_left_handed = true;
  s_sdk_type = ProcessAppSDKType_4x;
  BarFixture f;
  prv_fixture_init(&f, GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  action_bar_layer_set_follows_orientation(&f.bar, true);
  action_bar_layer_add_to_window(&f.bar, &f.window);
  const GRect bounds = GRect(7, 11, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT);
  GRect inset = action_bar_layer_inset_bounds(&f.bar, bounds);
  cl_assert_equal_i(inset.origin.x, bounds.origin.x + ACTION_BAR_WIDTH);
  cl_assert_equal_i(inset.origin.y, bounds.origin.y);
  cl_assert_equal_i(inset.size.w, bounds.size.w - ACTION_BAR_WIDTH);

  action_bar_layer_set_follows_orientation(&f.bar, false);
  inset = action_bar_layer_inset_bounds(&f.bar, bounds);
  cl_assert_equal_i(inset.origin.x, bounds.origin.x);
  cl_assert_equal_i(inset.size.w, bounds.size.w - ACTION_BAR_WIDTH);
  prv_fixture_deinit(&f);
}
