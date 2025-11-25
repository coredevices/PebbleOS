/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "pebble_asserts.h"

#include "applib/unobstructed_area_service.h"

// Stubs
/////////////////////
#include "stubs_app.h"
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_events.h"
#include "stubs_framebuffer.h"
#include "stubs_graphics.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_ui_window.h"

// Fakes
/////////////////////
#include "fake_event_service.h"

// Statics
/////////////////////

void *s_context_target;

typedef struct UnobstructedAreaTestData {
  void *context;
  int num_will_change_calls;
  int num_change_calls;
  int num_did_change_calls;
  GRect last_will_change_final_area;
  AnimationProgress last_change_progress;
} UnobstructedAreaTestData;

UnobstructedAreaTestData s_data;

static void prv_will_change(GRect final_area, void *context) {
  s_data.last_will_change_final_area = final_area;
  cl_assert_equal_p(context, s_data.context);
  s_data.num_will_change_calls++;
}

static void prv_change(AnimationProgress progress, void *context) {
  s_data.last_change_progress = progress;
  cl_assert_equal_p(context, s_data.context);
  s_data.num_change_calls++;
}

static void prv_did_change(void *context) {
  cl_assert_equal_p(context, s_data.context);
  s_data.num_did_change_calls++;
}

// Test boilerplate
/////////////////////

void test_unobstructed_area_service__initialize(void) {
  s_data = (UnobstructedAreaTestData) {
    .context = &s_context_target,
    .last_change_progress = -1,
  };

  fake_event_service_init();
  s_app_state_framebuffer = &(FrameBuffer) { .size = DISP_FRAME.size };

  unobstructed_area_service_init(app_state_get_unobstructed_area_state(), DISP_ROWS);
}

void test_unobstructed_area_service__cleanup(void) {
  app_unobstructed_area_service_unsubscribe();
  unobstructed_area_service_deinit(app_state_get_unobstructed_area_state());
}

// Tests
//////////////////////

void test_unobstructed_area_service__subscribe(void) {
  // Unsubscribing first should not crash
  app_unobstructed_area_service_unsubscribe();
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.will_change);
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.change);
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.did_change);

  // Subscribing should use the event service
  UnobstructedAreaHandlers handlers = {
    .will_change = prv_will_change,
    .change = prv_change,
    .did_change = prv_did_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.will_change, prv_will_change);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.change, prv_change);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.did_change, prv_did_change);

  // Unsubscribing after subscription should cancel subscriptions
  app_unobstructed_area_service_unsubscribe();
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.will_change);
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.change);
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.did_change);

  // Unsubscribing again should not crash
  app_unobstructed_area_service_unsubscribe();
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.will_change);
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.change);
  cl_assert(!app_state_get_unobstructed_area_state()->handlers.did_change);
}

void test_unobstructed_area_service__will_change(void) {
  UnobstructedAreaHandlers handlers = {
    .will_change = prv_will_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.will_change, prv_will_change);

  const GRect from_area = GRect(0, 0, DISP_COLS, 400);
  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  unobstructed_area_service_will_change(from_area.size.h, to_area.size.h);
  fake_event_service_handle_last();
  cl_assert_equal_i(s_data.num_will_change_calls, 1);
  const GRect to_area_expected = GRect(0, 0, DISP_COLS, MIN(200, DISP_ROWS));
  cl_assert_equal_grect(s_data.last_will_change_final_area, to_area_expected);
}

void test_unobstructed_area_service__will_change_twice(void) {
  UnobstructedAreaHandlers handlers = {
    .will_change = prv_will_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.will_change, prv_will_change);

  const GRect from_area = GRect(0, 0, DISP_COLS, 400);
  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  unobstructed_area_service_will_change(from_area.size.h, to_area.size.h);
  fake_event_service_handle_last();

  unobstructed_area_service_will_change(from_area.size.h, to_area.size.h);
  cl_assert_passert(fake_event_service_handle_last());
}

void test_unobstructed_area_service__change(void) {
  UnobstructedAreaHandlers handlers = {
    .change = prv_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.change, prv_change);

  const GRect from_area = GRect(0, 0, DISP_COLS, 400);
  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  unobstructed_area_service_will_change(from_area.size.h, to_area.size.h);
  fake_event_service_handle_last();

  const GRect area = GRect(0, 0, DISP_COLS, 200);
  const AnimationProgress progress = ANIMATION_NORMALIZED_MAX / 2;
  unobstructed_area_service_change(area.size.h, to_area.size.h, progress);
  fake_event_service_handle_last();
  cl_assert_equal_i(s_data.num_change_calls, 1);
  cl_assert_equal_i(s_data.last_change_progress, progress);
}

void test_unobstructed_area_service__change_after_subscribe(void) {
  const GRect from_area = GRect(0, 0, DISP_COLS, 400);
  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  unobstructed_area_service_will_change(from_area.size.h, to_area.size.h);

  UnobstructedAreaHandlers handlers = {
    .will_change = prv_will_change,
    .change = prv_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.change, prv_change);

  const GRect area = GRect(0, 0, DISP_COLS, 200);
  const AnimationProgress progress = ANIMATION_NORMALIZED_MAX / 2;
  unobstructed_area_service_change(area.size.h, to_area.size.h, progress);
  fake_event_service_handle_last();
  cl_assert_equal_i(s_data.num_will_change_calls, 1);
  cl_assert_equal_i(s_data.num_change_calls, 1);
  cl_assert_equal_i(s_data.last_change_progress, progress);
}

void test_unobstructed_area_service__change_no_will(void) {
  UnobstructedAreaHandlers handlers = {
    .will_change = prv_will_change,
    .change = prv_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.change, prv_change);

  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  const GRect area = GRect(0, 0, DISP_COLS, 200);
  const AnimationProgress progress = ANIMATION_NORMALIZED_MAX / 2;
  unobstructed_area_service_change(area.size.h, to_area.size.h, progress);
  fake_event_service_handle_last();
  cl_assert_equal_i(s_data.num_will_change_calls, 1);
  cl_assert_equal_i(s_data.num_change_calls, 1);
  cl_assert_equal_i(s_data.last_change_progress, progress);
}

void test_unobstructed_area_service__did_change(void) {
  UnobstructedAreaHandlers handlers = {
    .did_change = prv_did_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.did_change, prv_did_change);

  const GRect from_area = GRect(0, 0, DISP_COLS, 400);
  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  unobstructed_area_service_will_change(from_area.size.h, to_area.size.h);
  fake_event_service_handle_last();

  unobstructed_area_service_did_change(to_area.size.h);
  fake_event_service_handle_last();
  cl_assert_equal_i(s_data.num_did_change_calls, 1);
}

void test_unobstructed_area_service__did_change_after_subscribe(void) {
  const GRect from_area = GRect(0, 0, DISP_COLS, 400);
  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  unobstructed_area_service_will_change(from_area.size.h, to_area.size.h);

  UnobstructedAreaHandlers handlers = {
    .will_change = prv_will_change,
    .did_change = prv_did_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.did_change, prv_did_change);

  unobstructed_area_service_did_change(to_area.size.h);
  fake_event_service_handle_last();
  cl_assert_equal_i(s_data.num_will_change_calls, 1);
  cl_assert_equal_i(s_data.num_did_change_calls, 1);
}

void test_unobstructed_area_service__did_change_no_will(void) {
  UnobstructedAreaHandlers handlers = {
    .will_change = prv_will_change,
    .did_change = prv_did_change,
  };
  app_unobstructed_area_service_subscribe(handlers, s_data.context);
  cl_assert(fake_event_service_get_info(PEBBLE_UNOBSTRUCTED_AREA_EVENT)->handler);
  cl_assert_equal_p(app_state_get_unobstructed_area_state()->handlers.did_change, prv_did_change);

  const GRect to_area = GRect(0, 0, DISP_COLS, 200);
  unobstructed_area_service_did_change(to_area.size.h);
  fake_event_service_handle_last();
  cl_assert_equal_i(s_data.num_will_change_calls, 1);
  cl_assert_equal_i(s_data.num_did_change_calls, 1);
}

void test_unobstructed_area_service__layer_no_clip(void) {
  app_state_get_unobstructed_area_state()->area = GRect(0, 0, 400, 400);

  Layer root_layer = {
    .bounds = GRect(100, 100, 200, 200),
  };
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(&root_layer, &unobstructed_bounds);
  cl_assert_equal_grect(unobstructed_bounds, root_layer.bounds);
}

void test_unobstructed_area_service__layer_clip_x_y(void) {
  app_state_get_unobstructed_area_state()->area = GRect(0, 0, 400, 400);

  Layer root_layer = {
    .bounds = GRect(210, 220, 300, 300),
  };
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(&root_layer, &unobstructed_bounds);
  cl_assert_equal_grect(unobstructed_bounds, GRect(210, 220, 190, 180));
}

void test_unobstructed_area_service__layer_clip_nx_ny(void) {
  app_state_get_unobstructed_area_state()->area = GRect(0, 0, 400, 400);

  Layer root_layer = {
    .bounds = GRect(-110, -120, 300, 300),
  };
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(&root_layer, &unobstructed_bounds);
  cl_assert_equal_grect(unobstructed_bounds, GRect(0, 0, 190, 180));
}

void test_unobstructed_area_service__nested_layer_no_clip(void) {
  app_state_get_unobstructed_area_state()->area = GRect(0, 0, 400, 400);

  Layer root_layer = {
    .bounds = GRect(30, 30, 30, 30),
  };
  Layer layer = {
    .bounds = GRect(20, 20, 20, 20),
  };
  layer_add_child(&root_layer, &layer);
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(&layer, &unobstructed_bounds);
  cl_assert_equal_grect(unobstructed_bounds, layer.bounds);
}

void test_unobstructed_area_service__nested_layer_clip_x_y(void) {
  app_state_get_unobstructed_area_state()->area = GRect(0, 0, 400, 400);
  Layer root_layer = {
    .bounds = GRect(150, 120, 10, 10), // The size of the parent layer has no affect
  };
  Layer layer = {
    .bounds = GRect(110, 130, 300, 200),
  };
  layer_add_child(&root_layer, &layer);
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(&layer, &unobstructed_bounds);
  cl_assert_equal_grect(unobstructed_bounds, GRect(110, 130, 140, 150));
}

void test_unobstructed_area_service__nested_layer_clip_nx_ny(void) {
  app_state_get_unobstructed_area_state()->area = GRect(0, 0, 400, 400);
  Layer root_layer = {
    .bounds = GRect(-150, -120, 10, 10), // The size of the parent layer has no affect
  };
  Layer layer = {
    .bounds = GRect(-110, -130, 300, 290),
  };
  layer_add_child(&root_layer, &layer);
  GRect unobstructed_bounds;
  layer_get_unobstructed_bounds(&layer, &unobstructed_bounds);
  cl_assert_equal_grect(unobstructed_bounds, GRect(150, 120, 40, 40));
}
